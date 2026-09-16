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
// BL-765 WRITES THROUGH THAT SEAM, and adds three rows for what it changed:
//
//   D6  the fossil half reads the PAST. Generating the same body twice, once
//       with the continents result and once with it withheld (which leaves the
//       ground stationary at every epoch, so every palaeo answer collapses to
//       the present), must place coal and petroleum DIFFERENTLY. If it does not,
//       the palaeo query is wired in but not consumed, which is the failure mode
//       a seam like this actually has.
//   D6b the fossil half reads the PAST INTERIOR too (BL-961). Same body, same
//       seed, drift record present both times, but the second run has
//       Planetology's thermal series withheld — so the palaeo pre-pass reads
//       today's subsidence instead of the epoch's. The coal and petroleum
//       MAGNITUDES must differ somewhere. Presence is not asked to move: the
//       series moves by about a percent over the record's depth and says so
//       honestly, and a presence test would only pass by inflating it.
//   D7  the world still feeds itself. This is BL-762's deferred half: it stopped
//       because deleting the biological rows from the Body phase left a world
//       with no food, and survey_endowment reads agricultural_produce as a
//       region's farm score. So produce, timber and fibre must all reach tiles.
//   D8  and it still has energy — coal and petroleum on tiles, the other half of
//       what survey_endowment reads.
//
// The per-resource magnitudes are REPORTED, not asserted. Placement moved on
// purpose and a pinned band here would be a golden nobody authorised.
//
// Lua-free: it runs the shipped planetology -> continents -> tile pipeline and
// reads the generation record, without a world save or a Lua state.
// ---------------------------------------------------------------------------

#include "world/continents.hpp"
#include "world/planetology.hpp"
#include "world/resource_names.hpp"
#include "world/tile_generation.hpp"
#include "world/world.hpp"

#include <algorithm>
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
    /// Per-tile presence in raster order, for D6.
    std::vector<char> coal_map, oil_map;
    /// Per-tile magnitude in raster order, for D6b.
    std::vector<float> coal_val, oil_val;
};

run generate(uint32_t campaign_seed, bool with_drift = true, bool with_thermal = true)
{
    run out;
    const resolved_world rw = resolve_preferences(world_preferences{}, campaign_seed);
    body_inputs home = prototype_body(1);
    home.orbit_au = rw.home_orbit_au;

    const uint32_t body_seed = campaign_seed ^ prototype_body_seed(1);
    out.st = run_planetology(home, rw.params, body_seed);
    const continent_state cs = run_continents(out.st, gw, gh, body_seed ^ 0xC0117E57u);
    // D6b: withhold the thermal series and nothing else. The pre-pass treats an
    // empty series as "read the present", the same degraded answer a withheld
    // drift record gives, so this isolates the one term the series adds.
    if (!with_thermal) out.st.thermal_series.clear();

    world w;
    const entity_id body = w.create_entity();
    const std::vector<entity_id> ids =
        generate_body_tiles(w, body, gw, gh, out.st.profile, campaign_seed ^ 0xE471001u,
                            1.0f, &out.st, &out.rec, &cs.height_bias, &cs.convergent,
                            with_drift ? &cs : nullptr);

    for (entity_id id : ids)
    {
        if (id == null_entity) continue;
        const tile_component& t = w.tiles.at(id);
        for (std::size_t r = 0; r < resource_count; ++r)
            out.on_tiles[r] += static_cast<double>(t.resource_deposit[r]);
        // Raster-order presence, for the drift-vs-no-drift comparison. The tile
        // ids come back in raster order, so this vector is position-stable.
        out.coal_map.push_back(t.resource_deposit[static_cast<std::size_t>(resource_type::coal)] > 0.0f);
        out.oil_map.push_back(t.resource_deposit[static_cast<std::size_t>(resource_type::petroleum)] > 0.0f);
        out.coal_val.push_back(t.resource_deposit[static_cast<std::size_t>(resource_type::coal)]);
        out.oil_val.push_back(t.resource_deposit[static_cast<std::size_t>(resource_type::petroleum)]);
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

    // --- D6: the fossil half reads the PAST ---------------------------------
    // Same body, same seed, same everything except whether the ground is allowed
    // to have moved. If the two agree tile-for-tile, nothing is reading the
    // palaeo record.
    {
        const uint32_t seed = 0xABCDEF01u;
        const run drift = generate(seed, /*with_drift=*/true);
        const run still = generate(seed, /*with_drift=*/false);

        int coal_moved = 0, oil_moved = 0, land = 0;
        const std::size_t n = std::min(drift.coal_map.size(), still.coal_map.size());
        for (std::size_t i = 0; i < n; ++i)
        {
            ++land;
            if (drift.coal_map[i] != still.coal_map[i]) ++coal_moved;
            if (drift.oil_map[i]  != still.oil_map[i])  ++oil_moved;
        }
        std::printf("     drift vs stationary ground (seed %08X, %d tiles):"
                    " coal differs on %d, petroleum on %d\n",
                    seed, land, coal_moved, oil_moved);
        std::printf("     coal total %.0f -> %.0f, petroleum %.0f -> %.0f (stationary -> drifted)\n",
                    still.on_tiles[static_cast<std::size_t>(resource_type::coal)],
                    drift.on_tiles[static_cast<std::size_t>(resource_type::coal)],
                    still.on_tiles[static_cast<std::size_t>(resource_type::petroleum)],
                    drift.on_tiles[static_cast<std::size_t>(resource_type::petroleum)]);
        check(coal_moved > 0 || oil_moved > 0,
              "D6   the fossil half reads the PAST — withholding the drift record moves it");

        // --- D6b: and the past INTERIOR (BL-961) ------------------------------
        // Drift kept both times; only the thermal series is withheld. Magnitudes
        // are compared, not presence — see the header for why.
        {
            const run cold = generate(seed, /*with_drift=*/true, /*with_thermal=*/false);
            int coal_scaled = 0, oil_scaled = 0;
            const std::size_t m = std::min(drift.coal_val.size(), cold.coal_val.size());
            for (std::size_t i = 0; i < m; ++i)
            {
                if (drift.coal_val[i] != cold.coal_val[i]) ++coal_scaled;
                if (drift.oil_val[i]  != cold.oil_val[i])  ++oil_scaled;
            }
            const double c_with = drift.on_tiles[static_cast<std::size_t>(resource_type::coal)];
            const double c_sans = cold.on_tiles[static_cast<std::size_t>(resource_type::coal)];
            const double o_with = drift.on_tiles[static_cast<std::size_t>(resource_type::petroleum)];
            const double o_sans = cold.on_tiles[static_cast<std::size_t>(resource_type::petroleum)];
            std::printf("     thermal series vs present interior: coal magnitude differs on %d tiles,"
                        " petroleum on %d\n", coal_scaled, oil_scaled);
            std::printf("     coal total %.1f -> %.1f (x%.5f), petroleum %.1f -> %.1f (x%.5f)"
                        " (present -> epoch)\n",
                        c_sans, c_with, c_sans > 0.0 ? c_with / c_sans : 0.0,
                        o_sans, o_with, o_sans > 0.0 ? o_with / o_sans : 0.0);
            check(coal_scaled > 0 || oil_scaled > 0,
                  "D6b  the fossil half reads the past INTERIOR — withholding the thermal series moves it");
            check(drift.coal_map == cold.coal_map && drift.oil_map == cold.oil_map,
                  "D6b  ...as magnitude, not presence: WHERE the seams are is the drift record's call");
        }

        // --- D7 / D8: BL-762's deferred half -------------------------------
        const double produce = drift.on_tiles[static_cast<std::size_t>(resource_type::agricultural_produce)];
        const double timber  = drift.on_tiles[static_cast<std::size_t>(resource_type::timber)];
        const double fibre   = drift.on_tiles[static_cast<std::size_t>(resource_type::fibre)];
        const double coal    = drift.on_tiles[static_cast<std::size_t>(resource_type::coal)];
        const double oil     = drift.on_tiles[static_cast<std::size_t>(resource_type::petroleum)];
        std::printf("     food: produce %.0f, timber %.0f, fibre %.0f | energy: coal %.0f, petroleum %.0f\n",
                    produce, timber, fibre, coal, oil);
        check(produce > 0.0 && timber > 0.0 && fibre > 0.0,
              "D7   the world still feeds itself — the Body phase places no food and there IS food");
        check(coal > 0.0 && oil > 0.0,
              "D8   and it still has energy — coal and petroleum both reach tiles");
    }

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
