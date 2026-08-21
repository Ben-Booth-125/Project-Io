// ---------------------------------------------------------------------------
// Tile height retention (BL-517) — no SDL / Lua
// ---------------------------------------------------------------------------
// BL-517 retains tile generation Pass 1's heightmap on `tile_component::height`.
// The item's whole value is negative: it stops a number being thrown away, and
// it must not compute a new one. So the assertions here are shaped around the
// two ways that could quietly go wrong.
//
//   H1  CAPTURE, NOT RECOMPUTE. Every tile's `height` is BIT-IDENTICAL to the
//       corresponding element of `generation_record::height` — the value the
//       pass itself reports. A re-derivation that merely looked similar (a
//       re-run of the noise, a rescale, a float->double->float trip) fails this,
//       which is the point.
//   H2  THE FIELD IS REAL. Normalised into [0, 1], with genuine spread — a
//       capture that silently wrote zeros would pass H1 only if the record were
//       zeros too, so this is the independent sanity bar.
//   H3  THE RECORD IS NOT THE SOURCE. Generating with `record = nullptr` yields
//       the same per-tile heights bit-for-bit. If the capture depended on the
//       ledger being asked for, the field would be a lie on every real world
//       (the shipped generator passes no record).
//   H4  DETERMINISM. Two independent generations at the same seed produce
//       byte-identical heights AND byte-identical composition / landform /
//       deposits. The second half is the load-bearing one: it says the capture
//       did not perturb the RNG draw order it sits inside.
//   H5  NO TERRAIN RE-DERIVATION. Height and landform are not in lockstep — the
//       same landform class spans a wide height range and vice versa. This is a
//       standing guard on BL-517's explicit "do NOT re-derive landform from
//       height" scope line: if a later change wires height into Pass 5, the
//       classes collapse onto height bands and this check reports it.
//
// M   MEMORY, MEASURED not estimated: sizeof(tile_component) and the real
//     per-body byte cost of the new field are printed.
//
// Run: tile_height_retention

#include "world/continents.hpp"
#include "world/planetology.hpp"
#include "world/tile_generation.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int gw = 180, gh = 84; // the shipped homeworld grid
constexpr int k_cells = gw * gh;

int failures = 0;
int checks   = 0;

void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

/// Bit-exact float comparison. Deliberately not `==`: this harness exists to
/// catch a value that was recomputed rather than copied, and a recomputation
/// can land one ulp away while still comparing equal under a tolerance.
bool bit_equal(float a, float b)
{
    return std::memcmp(&a, &b, sizeof(float)) == 0;
}

struct generated
{
    world                   w;
    std::vector<entity_id>  ids;
    generation_record       rec;
};

/// One homeworld-shaped body. Mirrors the census's minimal path (planetology ->
/// continents -> tiles) without the BL-276 sea gate, which is irrelevant here:
/// every assertion is about one generation's internal consistency, not about
/// which candidate seed the shipped world would have accepted.
void generate_one(generated& g, uint32_t campaign_seed, bool want_record)
{
    const resolved_world rw = resolve_preferences(world_preferences{}, campaign_seed);
    body_inputs home = prototype_body(1);
    home.orbit_au = rw.home_orbit_au;

    const uint32_t body_seed = campaign_seed ^ prototype_body_seed(1);
    const planetology_state st = run_planetology(home, rw.params, body_seed);
    const continent_state   cs = run_continents(st, gw, gh, body_seed ^ 0xC0117E57u);

    const entity_id body = g.w.create_entity();
    g.ids = generate_body_tiles(g.w, body, gw, gh, st.profile,
                                campaign_seed ^ 0xE471001u, 1.0f, &st,
                                want_record ? &g.rec : nullptr,
                                &cs.height_bias, &cs.convergent);
}

const tile_component& tile_at(const generated& g, int idx)
{
    return g.w.tiles.at(g.ids[static_cast<std::size_t>(idx)]);
}

} // namespace

int main()
{
    std::printf("=== tile height retention (BL-517) ===\n");

    constexpr uint32_t seed = 20260821u;

    generated a;
    generate_one(a, seed, /*want_record=*/true);

    check(static_cast<int>(a.ids.size()) == k_cells,
          "G  the body generated a full " + std::to_string(k_cells) + "-tile grid");
    check(static_cast<int>(a.rec.height.size()) == k_cells,
          "G  the generation_record carries a height value per tile");

    // -- H1 -----------------------------------------------------------------
    {
        int mismatches = 0, first_bad = -1;
        for (int idx = 0; idx < k_cells; ++idx)
        {
            const float captured = tile_at(a, idx).height;
            const float recorded = a.rec.height[static_cast<std::size_t>(idx)];
            if (!bit_equal(captured, recorded))
            {
                ++mismatches;
                if (first_bad < 0) first_bad = idx;
            }
        }
        if (mismatches)
            std::printf("     first mismatch at raster %d: tile %.9g vs record %.9g\n",
                        first_bad, static_cast<double>(tile_at(a, first_bad).height),
                        static_cast<double>(a.rec.height[static_cast<std::size_t>(first_bad)]));
        check(mismatches == 0,
              "H1 every tile's height is BIT-IDENTICAL to generation_record::height ("
              + std::to_string(mismatches) + " mismatches)");
    }

    // -- H2 -----------------------------------------------------------------
    {
        float lo = 2.0f, hi = -1.0f;
        double sum = 0.0;
        std::vector<float> distinct;
        distinct.reserve(static_cast<std::size_t>(k_cells));
        for (int idx = 0; idx < k_cells; ++idx)
        {
            const float h = tile_at(a, idx).height;
            lo = std::min(lo, h);
            hi = std::max(hi, h);
            sum += static_cast<double>(h);
            distinct.push_back(h);
        }
        std::sort(distinct.begin(), distinct.end());
        const std::size_t uniq =
            static_cast<std::size_t>(std::unique(distinct.begin(), distinct.end()) - distinct.begin());

        check(lo >= 0.0f && hi <= 1.0f,
              "H2 heights are normalised into [0, 1]");
        check(hi - lo > 0.5f,
              "H2 the field has real spread, not a constant");
        check(uniq > static_cast<std::size_t>(k_cells) / 2,
              "H2 heights are per-tile distinct values, not a handful of bands");
        std::printf("     height min %.4f  max %.4f  mean %.4f  distinct %zu / %d\n",
                    static_cast<double>(lo), static_cast<double>(hi),
                    sum / static_cast<double>(k_cells), uniq, k_cells);
    }

    // -- H3 -----------------------------------------------------------------
    {
        generated b;
        generate_one(b, seed, /*want_record=*/false);

        int mismatches = 0;
        for (int idx = 0; idx < k_cells; ++idx)
            if (!bit_equal(tile_at(a, idx).height, tile_at(b, idx).height))
                ++mismatches;

        check(mismatches == 0,
              "H3 heights are identical when generated with NO generation_record ("
              + std::to_string(mismatches) + " mismatches)");
    }

    // -- H4 -----------------------------------------------------------------
    {
        generated c;
        generate_one(c, seed, /*want_record=*/true);

        int h_bad = 0, terrain_bad = 0, deposit_bad = 0;
        for (int idx = 0; idx < k_cells; ++idx)
        {
            const tile_component& t0 = tile_at(a, idx);
            const tile_component& t1 = tile_at(c, idx);
            if (!bit_equal(t0.height, t1.height)) ++h_bad;
            if (t0.composition != t1.composition || t0.landform != t1.landform) ++terrain_bad;
            if (std::memcmp(t0.resource_deposit.data(), t1.resource_deposit.data(),
                            sizeof(t0.resource_deposit)) != 0)
                ++deposit_bad;
        }

        check(h_bad == 0, "H4 the same seed reproduces byte-identical heights");
        check(terrain_bad == 0,
              "H4 composition + landform are byte-identical across runs (capture drew no RNG)");
        check(deposit_bad == 0,
              "H4 deposit arrays are byte-identical across runs (draw order unperturbed)");
    }

    // -- H5 -----------------------------------------------------------------
    {
        // Per-landform height range. If a later change re-derived landform from
        // height, each class would occupy a narrow band and the ranges would stop
        // overlapping. Measured as: does the widest class span most of [0, 1],
        // and do at least two classes overlap substantially?
        constexpr int k_forms = 8;
        float lo[k_forms], hi[k_forms];
        int   count[k_forms] = {};
        for (int i = 0; i < k_forms; ++i) { lo[i] = 2.0f; hi[i] = -1.0f; }

        for (int idx = 0; idx < k_cells; ++idx)
        {
            const tile_component& t = tile_at(a, idx);
            const int f = static_cast<int>(t.landform);
            if (f < 0 || f >= k_forms) continue;
            lo[f] = std::min(lo[f], t.height);
            hi[f] = std::max(hi[f], t.height);
            ++count[f];
        }

        float widest = 0.0f;
        int   populated = 0;
        for (int i = 0; i < k_forms; ++i)
            if (count[i] > 0)
            {
                ++populated;
                widest = std::max(widest, hi[i] - lo[i]);
                std::printf("     landform %d: %6d tiles, height %.3f .. %.3f\n",
                            i, count[i], static_cast<double>(lo[i]), static_cast<double>(hi[i]));
            }

        check(populated >= 2, "H5 more than one landform class was generated");
        check(widest > 0.6f,
              "H5 landform is NOT a function of height — the widest class spans most of the range");
    }

    // -- M ------------------------------------------------------------------
    {
        const std::size_t sz = sizeof(tile_component);
        const std::size_t field_bytes = sizeof(float) * static_cast<std::size_t>(k_cells);
        std::printf("\nM  sizeof(tile_component) = %zu bytes\n", sz);
        std::printf("M  height field cost on this %d-tile body = %zu bytes (%.1f KB)\n",
                    k_cells, field_bytes, static_cast<double>(field_bytes) / 1024.0);
        std::printf("M  whole tile map on this body = %zu bytes (%.1f KB)\n",
                    sz * static_cast<std::size_t>(k_cells),
                    static_cast<double>(sz * static_cast<std::size_t>(k_cells)) / 1024.0);
    }

    std::printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
