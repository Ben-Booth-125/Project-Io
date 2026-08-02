#include "river_generation.hpp"

#include <algorithm>
#include <random>

namespace {

// Odd-r offset hex neighbour tables, direction order E, NE, NW, W, SW, SE — mirrors
// placement_rules.cpp's is_coastal tables. Tabulated independently here to keep this
// pass a self-contained sibling (BL-051 convention) rather than reaching into
// placement_rules for a shared constant.
constexpr int even_off[6][2] = { {+1, 0}, {0, -1}, {-1, -1}, {-1, 0}, {-1, +1}, {0, +1} };
constexpr int odd_off[6][2]  = { {+1, 0}, {+1, -1}, {0, -1}, {-1, 0}, {0, +1}, {+1, +1} };

inline int raster_idx(int col, int row, int gw)
{
    return ((col % gw) + gw) % gw + row * gw;
}

} // namespace

int hex_side_for_offset(int dc, int dr, bool odd_row)
{
    const auto& tbl = odd_row ? odd_off : even_off;
    for (int i = 0; i < 6; ++i)
        if (tbl[i][0] == dc && tbl[i][1] == dr)
            return i;
    return -1;
}

bool tile_borders_river(const tile_component& tc)
{
    return tc.river_edges != 0;
}

float river_edge_discount(const tile_component& from, int side)
{
    if (side < 0 || side > 5)
        return 1.0f;
    const auto bit = static_cast<std::uint8_t>(1u << side);
    if ((from.river_edges & bit) == 0)
        return 1.0f;

    // Downstream travel (with the current) is cheaper than upstream (against it).
    // Magnitudes chosen to sit within the road-tier discount's scale
    // (road_traversal_multiplier: Track ~0.67, Road 0.50, Highway 0.40) without
    // outstripping it — a river is a bonus lane, not a replacement for the road network.
    constexpr float downstream_mult = 0.75f; // 25% cheaper
    constexpr float upstream_mult   = 0.85f; // 15% cheaper
    return (from.river_downstream & bit) ? downstream_mult : upstream_mult;
}

void generate_rivers(world& w, const std::vector<entity_id>& tile_ids,
                     int gw, int gh, const std::vector<float>& height, uint32_t seed)
{
    const int total = gw * gh;
    if (gw <= 0 || gh <= 0
        || static_cast<int>(tile_ids.size()) < total
        || static_cast<int>(height.size()) < total)
        return;

    const auto tc_at = [&](int idx) -> tile_component* {
        if (idx < 0 || idx >= total)
            return nullptr;
        const entity_id tid = tile_ids[static_cast<std::size_t>(idx)];
        if (tid == null_entity)
            return nullptr;
        const auto it = w.tiles.find(tid);
        return (it != w.tiles.end()) ? &it->second : nullptr;
    };

    // Source candidates: high-ground non-ocean land (mountain/highland landform).
    // Sorted by (height desc, raster idx asc) so candidate order is a pure function
    // of the height field and grid shape, independent of w.tiles' iteration order.
    std::vector<int> candidates;
    candidates.reserve(static_cast<std::size_t>(total) / 8 + 1);
    for (int idx = 0; idx < total; ++idx)
    {
        const tile_component* tc = tc_at(idx);
        if (!tc || tc->composition == terrain_composition::ocean)
            continue;
        if (tc->landform == terrain_landform::mountain || tc->landform == terrain_landform::highland)
            candidates.push_back(idx);
    }
    if (candidates.empty())
        return;
    std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
        const float ha = height[static_cast<std::size_t>(a)];
        const float hb = height[static_cast<std::size_t>(b)];
        if (ha != hb) return ha > hb;
        return a < b;
    });

    // Number of rivers scales with grid area — a fresh, self-contained formula (this
    // is a sibling pass, not a Pass-5 dependency on scale_to_area).
    const int num_rivers = std::clamp(total / 1800, 3, 24);

    std::mt19937 rng(seed ^ 0x52495645u); // "RIVE"

    // Deterministic pick: walk the sorted candidate list in fixed strides, jittering
    // the offset within each stride with the seeded rng, so source draws are a pure
    // function of (seed, candidates) regardless of any container's iteration order.
    std::vector<int> sources;
    sources.reserve(static_cast<std::size_t>(num_rivers));
    const int stride = std::max<int>(1, static_cast<int>(candidates.size()) / num_rivers);
    for (int i = 0; i < num_rivers && i * stride < static_cast<int>(candidates.size()); ++i)
    {
        const int base = i * stride;
        const int span = std::min(stride, static_cast<int>(candidates.size()) - base);
        const int off  = span > 1 ? static_cast<int>(rng() % static_cast<unsigned>(span)) : 0;
        sources.push_back(candidates[static_cast<std::size_t>(base + off)]);
    }

    for (const int src_idx : sources)
    {
        int cur = src_idx;
        // Strictly-decreasing height per step guarantees termination (no cycles
        // possible) in at most `total` steps.
        for (int steps = 0; steps < total; ++steps)
        {
            tile_component* cur_tc = tc_at(cur);
            if (!cur_tc)
                break;

            const int col = cur % gw;
            const int row = cur / gw;
            const bool odd_row = (row & 1) != 0;
            const auto& tbl = odd_row ? odd_off : even_off;

            int best_side = -1;
            int best_idx  = -1;
            float best_h  = height[static_cast<std::size_t>(cur)];
            for (int side = 0; side < 6; ++side)
            {
                const int nrow = row + tbl[side][1];
                if (nrow < 0 || nrow >= gh)
                    continue;
                const int ncol = ((col + tbl[side][0]) % gw + gw) % gw;
                const int nidx = raster_idx(ncol, nrow, gw);
                if (!tc_at(nidx))
                    continue;
                const float nh = height[static_cast<std::size_t>(nidx)];
                if (nh < best_h)
                {
                    best_h    = nh;
                    best_side = side;
                    best_idx  = nidx;
                }
            }

            if (best_side < 0)
                break; // local basin — trace ends here; no lake tile is created (out of scope).

            tile_component* n_tc = tc_at(best_idx);
            const int opp_side = (best_side + 3) % 6;
            const auto bit_cur = static_cast<std::uint8_t>(1u << best_side);
            const auto bit_n   = static_cast<std::uint8_t>(1u << opp_side);

            cur_tc->river_edges     |= bit_cur;
            cur_tc->river_downstream |= bit_cur; // outflow from cur
            n_tc->river_edges       |= bit_n;
            n_tc->river_downstream  &= static_cast<std::uint8_t>(~bit_n); // inflow into n

            if (n_tc->composition == terrain_composition::ocean)
                break; // reached the sea — trace complete.

            cur = best_idx;
        }
    }
}
