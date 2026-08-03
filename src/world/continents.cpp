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

    // --- Rift-basin sea (BL-276). The largest divergent boundary between two
    //     CONTINENTAL plates founders: a strong negative bias, falling off with
    //     distance from the boundary corridor, drops it below Pass 2's ocean
    //     percentile so the rift floods into an inland sea. The threshold is a
    //     percentile of water_fraction, so the basin RELOCATES ocean rather
    //     than adding any. Worlds whose plate draw yields no such pair get no
    //     basin — the deliberately-hard tail Ben wants kept (2026-08-03).
    //     Draws nothing from the shared stream `r`, so every value computed
    //     above is bit-identical to the pre-BL-276 pass. ---
    {
        // Gather every candidate corridor (divergent, both plates continental),
        // then choose the most INLAND one: score by how continental the tiles
        // around the corridor are. A rift that opens mid-ocean just makes one
        // more ocean arm; a rift in a continental interior makes the sea.
        auto corridor_of = [&](int a, int b)
        {
            std::vector<std::pair<float, float>> c; // (col, row)
            for (int row = 0; row < gh; ++row)
            {
                for (int col = 0; col < gw; ++col)
                {
                    const int here  = plate_id[static_cast<std::size_t>(col + row * gw)];
                    const int right = plate_id[static_cast<std::size_t>(((col + 1) % gw) + row * gw)];
                    const int down  = (row + 1 < gh) ? plate_id[static_cast<std::size_t>(col + (row + 1) * gw)] : here;
                    for (const int other : { right, down })
                    {
                        if (std::min(here, other) == a && std::max(here, other) == b)
                        {
                            c.emplace_back(static_cast<float>(col), static_cast<float>(row));
                            break;
                        }
                    }
                }
            }
            return c;
        };

        // Per-tile inland-ness: fraction of a sampling box around one tile owned
        // by continental plates (plate ownership stands in for land, since land
        // follows the continental +0.05 base bias). A corridor tile that sits in
        // mostly-oceanic surroundings would flood into the world ocean and
        // breach the rift-shoulder rim, so only the land-interior SEGMENT of a
        // corridor is stamped.
        auto tile_inlandness = [&](float cc, float cr)
        {
            int cont = 0, tot = 0;
            constexpr int reach = 8;
            for (int dr = -reach; dr <= reach; dr += 4)
            {
                const int row = static_cast<int>(cr) + dr;
                if (row < 0 || row >= gh) continue;
                for (int dc = -reach; dc <= reach; dc += 4)
                {
                    const int col = ((static_cast<int>(cc) + dc) % gw + gw) % gw;
                    ++tot;
                    if (!out.plates[static_cast<std::size_t>(
                            plate_id[static_cast<std::size_t>(col + row * gw)])].oceanic)
                        ++cont;
                }
            }
            return tot ? static_cast<float>(cont) / static_cast<float>(tot) : 0.0f;
        };

        auto interior_segment = [&](const std::vector<std::pair<float, float>>& c)
        {
            std::vector<std::pair<float, float>> keep;
            for (const auto& [cc, cr] : c)
                if (tile_inlandness(cc, cr) >= 0.75f) keep.emplace_back(cc, cr);
            return keep;
        };

        // Choose the pair with the LONGEST land-interior segment — corridor
        // length in open ocean earns nothing.
        int best_a = -1, best_b = -1;
        std::vector<std::pair<float, float>> corridor;
        for (int a = 0; a < plate_count; ++a)
        {
            for (int b = a + 1; b < plate_count; ++b)
            {
                if (out.plates[static_cast<std::size_t>(a)].oceanic ||
                    out.plates[static_cast<std::size_t>(b)].oceanic) continue;
                const int key = a * plate_count + b;
                if (boundary_sign[static_cast<std::size_t>(key)] <= 0) continue; // divergent only
                if (boundary_tiles[static_cast<std::size_t>(key)] < notable_threshold) continue;
                auto seg = interior_segment(corridor_of(a, b));
                if (static_cast<int>(seg.size()) > static_cast<int>(corridor.size()))
                {
                    best_a = a; best_b = b;
                    corridor = std::move(seg);
                }
            }
        }

        // A basin needs a real interior segment to flood; a couple of tiles
        // would stamp a pond, not a sea.
        if (static_cast<int>(corridor.size()) < 6) best_a = -1;

        // Fallback for worlds whose plate draw yields no continental divergent
        // pair: an INTRACRATONIC SAG basin (the Caspian shape) — the most
        // inland continental point bows down and an inland sea gathers. Still a
        // pure consequence of the plate layout (the point is the inland-ness
        // argmax, no draw), so the deliberately-hard tail that remains is the
        // worlds where even the stamp fails against the noise, not a quota.
        bool sag_basin = false;
        if (best_a < 0)
        {
            float best_inland = 0.70f; // below this, nowhere is truly inland — no basin.
            for (int row = 2; row < gh - 2; row += 3)
            {
                for (int col = 0; col < gw; col += 3)
                {
                    if (out.plates[static_cast<std::size_t>(
                            plate_id[static_cast<std::size_t>(col + row * gw)])].oceanic) continue;
                    const float s = tile_inlandness(static_cast<float>(col), static_cast<float>(row));
                    if (s > best_inland)
                    {
                        best_inland = s;
                        corridor.assign(1, { static_cast<float>(col), static_cast<float>(row) });
                        sag_basin = true;
                    }
                }
            }
        }

        if (best_a >= 0 || sag_basin)
        {
            // Distance-falloff stamp. Inside basin_radius the corridor founders
            // (deepest at the rift axis); between basin_radius and rim_radius
            // the RIFT SHOULDER uplifts — real rift geology, and the thing that
            // seals the flooded basin off from the world ocean so it reads as
            // an enclosed sea rather than one more ocean arm.
            // Adaptive width: a short interior rift floods WIDE (a Black-Sea
            // oval), a long one stays a narrow elongated sea — both aim at
            // roughly the same flooded area, so a playable arena does not
            // depend on the luck of the segment's length.
            const float seg_len = static_cast<float>(corridor.size());
            const float basin_radius = std::clamp(260.0f / seg_len, 4.5f, 11.0f);
            const float rim_radius   = basin_radius + 5.0f;
            constexpr float basin_depth = 0.65f;
            constexpr float rim_height  = 0.50f;
            for (int row = 0; row < gh; ++row)
            {
                for (int col = 0; col < gw; ++col)
                {
                    float best_d2 = rim_radius * rim_radius;
                    for (const auto& [cc, cr] : corridor)
                    {
                        const float dc = wrapped_dcol(static_cast<float>(col), cc, gw);
                        const float dr = static_cast<float>(row) - cr;
                        const float d2 = dc * dc + dr * dr;
                        if (d2 < best_d2) best_d2 = d2;
                    }
                    if (best_d2 >= rim_radius * rim_radius) continue;
                    const float d = std::sqrt(best_d2);
                    float& bias = out.height_bias[static_cast<std::size_t>(col + row * gw)];
                    if (d < basin_radius)
                        bias -= basin_depth * (1.0f - d / basin_radius);
                    else
                        bias += rim_height * (1.0f - (d - basin_radius) / (rim_radius - basin_radius));
                }
            }

            // Dated like the pair lines, but on its own local stream tag so the
            // existing per-pair draws are untouched.
            const uint32_t basin_tag = sag_basin
                ? 0x5A6BA51u
                : static_cast<uint32_t>((best_a * plate_count + best_b) * 0x1000193u);
            rng br(seed, tag_continents ^ 0xBA51Bu ^ basin_tag);
            const float gya = 0.1f + br.unit() * 1.5f;
            events.push_back(history_event{
                years_from_gya(gya), chain_stage::engine,
                sag_basin ? "The craton's interior sagged into a broad basin."
                          : "A continental rift foundered below the waterline.",
                sag_basin ? "-> an inland sea gathered where the land bowed"
                          : "-> an enclosed inland sea floods the basin; sheltered coasts on every shore"
            });
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
