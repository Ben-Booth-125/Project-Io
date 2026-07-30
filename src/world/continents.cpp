#include "continents.hpp"

#include <algorithm>
#include <cmath>

namespace {

// ---------------------------------------------------------------------------
// Deterministic RNG — same splitmix64 shape as planetology.cpp's local `rng`,
// duplicated rather than shared (each generation file owns its stream, per
// existing convention in this codebase; e.g. tile_generation.cpp's own noise).
// ---------------------------------------------------------------------------
uint64_t splitmix64(uint64_t x)
{
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

struct rng
{
    uint64_t s;
    rng(uint32_t seed, uint32_t stage_tag)
        : s(splitmix64((static_cast<uint64_t>(seed) << 32) ^ (stage_tag * 0x9E3779B1u))) {}

    float unit() // [0,1)
    {
        s = splitmix64(s);
        return static_cast<float>((s >> 40) & 0xFFFFFFull) * (1.0f / 16777216.0f);
    }
    int index(int n) { return std::min(n - 1, static_cast<int>(unit() * static_cast<float>(n))); }
};

constexpr uint32_t tag_continents = 0xC017u;

// Fixed 8-way compass, NOT sin/cos — literal constants are compiled once and
// are bit-identical everywhere, unlike a runtime trig call (PLANETOLOGY.md
// § Determinism & cost names the transcendental hazard).
constexpr float compass_dx[8] = { 1.0f,  0.7071f,  0.0f, -0.7071f, -1.0f, -0.7071f,  0.0f,  0.7071f };
constexpr float compass_dy[8] = { 0.0f,  0.7071f,  1.0f,  0.7071f,  0.0f, -0.7071f, -1.0f, -0.7071f };

/// Wrapped column distance (the tile grid wraps horizontally, per
/// TILE_GENERATION.md's cylinder convention).
float wrapped_dcol(float a, float b, int gw)
{
    float d = std::fabs(a - b);
    return std::min(d, static_cast<float>(gw) - d);
}

} // namespace

continent_state run_continents(const planetology_state& pl, int gw, int gh, uint32_t seed)
{
    continent_state out;
    const int total = gw * gh;
    out.height_bias.assign(static_cast<std::size_t>(total), 0.0f);
    // Sized before the stagnant-lid early return so every consumer sees a
    // full-length field: one immobile plate owns tile 0..n, hence all-zero.
    out.plate_id.assign(static_cast<std::size_t>(total), 0);

    // --- Plate count: a CONSEQUENCE of Engine's already-computed budget, not
    //     an independent roll. A stagnant lid drifts as one immobile plate. ---
    const int plate_count = pl.mobile_lid
        ? std::clamp(static_cast<int>(std::lround(4.0f + pl.theta * 3.0f)), 4, 10)
        : 1;

    rng r(seed, tag_continents);

    out.plates.reserve(static_cast<std::size_t>(plate_count));
    for (int i = 0; i < plate_count; ++i)
    {
        tectonic_plate p;
        p.seed_col = r.unit() * static_cast<float>(gw);
        p.seed_row = r.unit() * static_cast<float>(gh);
        const int dir = r.index(8);
        // Drift speed is a consequence of the same thermal budget that set
        // plate count — more heat, faster convection, bigger boundary effects.
        const float speed = std::clamp(pl.theta * 0.6f, 0.15f, 1.2f);
        p.drift_col = compass_dx[dir] * speed;
        p.drift_row = compass_dy[dir] * speed;
        p.oceanic   = r.unit() < 0.4f;
        out.plates.push_back(p);
    }

    if (plate_count == 1)
    {
        out.history.push_back(history_event{
            years_from_gya(4.50f), chain_stage::engine,
            "Interior locked into a single stagnant plate.",
            "-> no subduction, no porphyry copper; terrain reflects only impact and volcanic history"
        });
        return out;
    }

    // --- Voronoi assignment: nearest plate per tile, wrapped in x. ---
    std::vector<int>& plate_id = out.plate_id;
    for (int row = 0; row < gh; ++row)
    {
        for (int col = 0; col < gw; ++col)
        {
            int   best    = 0;
            float best_d2 = 1e30f;
            for (int i = 0; i < plate_count; ++i)
            {
                const float dc = wrapped_dcol(static_cast<float>(col), out.plates[static_cast<std::size_t>(i)].seed_col, gw);
                const float dr = static_cast<float>(row) - out.plates[static_cast<std::size_t>(i)].seed_row;
                const float d2 = dc * dc + dr * dr;
                if (d2 < best_d2) { best_d2 = d2; best = i; }
            }
            const int idx = col + row * gw;
            plate_id[static_cast<std::size_t>(idx)] = best;
            out.height_bias[static_cast<std::size_t>(idx)] =
                out.plates[static_cast<std::size_t>(best)].oceanic ? -0.10f : 0.05f;
        }
    }

    // --- Boundary classification: convergent uplift / divergent rift. ---
    // boundary_tiles[a * plate_count + b] (a < b) counts shared-edge tiles;
    // used only to decide which pairs are notable enough for a history line.
    std::vector<int> boundary_tiles(static_cast<std::size_t>(plate_count) * static_cast<std::size_t>(plate_count), 0);
    std::vector<int> boundary_sign(static_cast<std::size_t>(plate_count) * static_cast<std::size_t>(plate_count), 0);

    auto classify_edge = [&](int a, int b)
    {
        if (a == b) return;
        const tectonic_plate& pa = out.plates[static_cast<std::size_t>(a)];
        const tectonic_plate& pb = out.plates[static_cast<std::size_t>(b)];
        const float to_b_col = wrapped_dcol(pb.seed_col, pa.seed_col, gw) *
                               ((pb.seed_col >= pa.seed_col) ? 1.0f : -1.0f);
        const float to_b_row = pb.seed_row - pa.seed_row;
        const float rel_col  = pa.drift_col - pb.drift_col;
        const float rel_row  = pa.drift_row - pb.drift_row;
        const float dot      = rel_col * to_b_col + rel_row * to_b_row;

        const int key = std::min(a, b) * plate_count + std::max(a, b);
        boundary_tiles[static_cast<std::size_t>(key)] += 1;
        // dot < 0: A drifts toward B faster than B recedes -> closing -> convergent.
        // dot > 0: opening -> divergent. Near zero: transform, no net bias.
        if (dot < -0.01f)      boundary_sign[static_cast<std::size_t>(key)] -= 1;
        else if (dot > 0.01f)  boundary_sign[static_cast<std::size_t>(key)] += 1;
    };

    for (int row = 0; row < gh; ++row)
    {
        for (int col = 0; col < gw; ++col)
        {
            const int idx  = col + row * gw;
            const int here = plate_id[static_cast<std::size_t>(idx)];

            const int right = plate_id[static_cast<std::size_t>(((col + 1) % gw) + row * gw)];
            if (right != here)
            {
                classify_edge(here, right);
            }
            if (row + 1 < gh)
            {
                const int down = plate_id[static_cast<std::size_t>(col + (row + 1) * gw)];
                if (down != here) classify_edge(here, down);
            }
        }
    }

    // Re-walk boundary tiles to apply the per-pair bias now that classification
    // is known (uplift for convergent, subsidence for divergent, nothing for
    // transform-dominant pairs).
    constexpr int    notable_threshold = 5;
    std::vector<history_event> events;

    for (int a = 0; a < plate_count; ++a)
    {
        for (int b = a + 1; b < plate_count; ++b)
        {
            const int key   = a * plate_count + b;
            const int count = boundary_tiles[static_cast<std::size_t>(key)];
            if (count < notable_threshold) continue;

            const int sign = boundary_sign[static_cast<std::size_t>(key)];
            if (sign == 0) continue; // transform-dominant: no net bias, no line.

            const bool convergent = sign < 0;
            // A synthetic but deterministic timestamp for the narrative, spread
            // across deep time by hashing the pair rather than all reading "now".
            rng er(seed, tag_continents ^ static_cast<uint32_t>(key * 0x1000193u));
            const float gya = 0.3f + er.unit() * 3.8f;

            events.push_back(history_event{
                years_from_gya(gya), chain_stage::engine,
                convergent ? "Two plates collided along a long-lived boundary."
                           : "A boundary pulled apart into a spreading rift.",
                convergent ? "-> mountain range and arc magmatism; porphyry copper where it persists"
                           : "-> rift basin and new coastline"
            });
        }
    }

    // Apply the per-boundary bias to the tiles that touch a classified pair.
    for (int row = 0; row < gh; ++row)
    {
        for (int col = 0; col < gw; ++col)
        {
            const int idx  = col + row * gw;
            const int here = plate_id[static_cast<std::size_t>(idx)];
            const int right = plate_id[static_cast<std::size_t>(((col + 1) % gw) + row * gw)];
            const int down  = (row + 1 < gh) ? plate_id[static_cast<std::size_t>(col + (row + 1) * gw)] : here;

            for (const int other : { right, down })
            {
                if (other == here) continue;
                const int a = std::min(here, other), b = std::max(here, other);
                const int key = a * plate_count + b;
                if (boundary_tiles[static_cast<std::size_t>(key)] < notable_threshold) continue;
                const int sign = boundary_sign[static_cast<std::size_t>(key)];
                if (sign < 0)      out.height_bias[static_cast<std::size_t>(idx)] += 0.12f; // convergent
                else if (sign > 0) out.height_bias[static_cast<std::size_t>(idx)] -= 0.08f; // divergent
            }
        }
    }

    // Chronological (oldest first), matching the biography's dated-line convention.
    // stable_sort, not sort: two boundaries can hash to the same year, and
    // std::sort leaves tied elements in an unspecified order — a determinism
    // hazard the float key merely made unlikely rather than impossible. The
    // preserved order is the plate-pair key walk above, which is deterministic.
    std::stable_sort(events.begin(), events.end(),
                     [](const history_event& x, const history_event& y)
                     { return x.years_before_epoch > y.years_before_epoch; });
    out.history = std::move(events);

    return out;
}
