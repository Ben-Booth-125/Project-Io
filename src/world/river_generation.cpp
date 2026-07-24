#include "river_generation.hpp"

#include "components.hpp"
#include "logistics.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace {

// The 6 hex neighbours of (col, row), one per fixed cube direction, wrapped at
// gw on the column axis; a direction whose row falls outside [0, gh) is marked
// absent (first == -1). Deliberately NOT tile_generation.cpp's odd-r offset
// table: that table is not geometrically reciprocal (its per-row-parity
// dc/dr pairs do not invert cleanly, which is harmless for its BFS cluster
// growth but breaks a symmetric bit encoding). This version converts odd-r
// offset -> cube coordinates, walks one of the 6 standard cube direction
// vectors, and converts back — by construction the neighbour reached via
// direction i always lists this tile back via direction (i+3)%6, which is
// exactly the invariant tile_component.river_edges needs (bit i set on tile A
// toward B implies bit (i+3)%6 set on B toward A, with no per-edge lookup).
void hex_neighbours(int col, int row, int gw, int gh,
                    std::pair<int, int> out[6], int& count)
{
    static constexpr int dx[6] = {  1,  1,  0, -1, -1,  0 };
    static constexpr int dz[6] = {  0, -1, -1,  0,  1,  1 };

    const int x = col - (row - (row & 1)) / 2;
    const int z = row;

    count = 0;
    for (int i = 0; i < 6; ++i)
    {
        const int nx = x + dx[i];
        const int nz = z + dz[i];
        if (nz < 0 || nz >= gh)
        {
            out[i] = { -1, -1 };
            continue;
        }
        const int ncol = nx + (nz - (nz & 1)) / 2;
        const int wcol = ((ncol % gw) + gw) % gw;
        out[i] = { wcol, nz };
        ++count;
    }
}

// Opposite direction index (reciprocal by construction — see hex_neighbours).
inline int opposite_direction(int i) { return (i + 3) % 6; }

// Source tiles must clear this normalised-height threshold (record.height, same
// field Pass 1/2 use) to seed a river — keeps rivers to genuine high ground
// rather than every tile with a marginally-lower neighbour.
constexpr float kSourceHeightThreshold = 0.62f;

// Minimum grid (wrap-aware column) separation between two chosen river sources,
// so a single ridge does not spawn a dense cluster of parallel rivers.
constexpr int kMinSourceSeparationSq = 36; // 6 tiles

// Hard cap on trace length — a guard against a pathological flat/plateau field
// producing a very long wandering path; real terrain drains well inside this.
constexpr int kMaxRiverLength = 400;

} // namespace

void generate_rivers(world& w, entity_id body, const generation_record& record)
{
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end())
        return;
    const int gw = bit->second.grid_width;
    const int gh = bit->second.grid_height;
    if (gw <= 0 || gh <= 0)
        return;
    const int total = gw * gh;
    if (record.height.size() != static_cast<std::size_t>(total)
        || record.gw != gw || record.gh != gh)
        return; // record wasn't captured for this body/grid — nothing to trace from

    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    if (static_cast<int>(grid.size()) != total)
        return;

    const auto height_at = [&](int idx) { return record.height[static_cast<std::size_t>(idx)]; };
    const auto tile_at = [&](int idx) -> tile_component* {
        const entity_id tid = grid[static_cast<std::size_t>(idx)];
        if (tid == null_entity)
            return nullptr;
        const auto tit = w.tiles.find(tid);
        return (tit != w.tiles.end()) ? &tit->second : nullptr;
    };
    const auto is_ocean = [&](int idx) {
        const tile_component* tc = tile_at(idx);
        return tc == nullptr || tc->composition == terrain_composition::ocean;
    };

    // 1. Candidate sources: land tiles at/above the height threshold, ordered by
    //    height descending then tile index ascending (fully deterministic order).
    std::vector<int> candidates;
    for (int idx = 0; idx < total; ++idx)
        if (!is_ocean(idx) && height_at(idx) >= kSourceHeightThreshold)
            candidates.push_back(idx);
    std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
        if (height_at(a) != height_at(b))
            return height_at(a) > height_at(b);
        return a < b;
    });

    // 2. Greedily keep spaced-out sources (skip a candidate too close to one
    //    already chosen — wrap-aware on the column axis).
    std::vector<int> sources;
    for (int idx : candidates)
    {
        const int col = idx % gw, row = idx / gw;
        bool too_close = false;
        for (int s : sources)
        {
            const int scol = s % gw, srow = s / gw;
            int dc = std::abs(col - scol);
            dc = std::min(dc, gw - dc);
            const int dr = row - srow;
            if (dc * dc + dr * dr < kMinSourceSeparationSq)
            {
                too_close = true;
                break;
            }
        }
        if (!too_close)
            sources.push_back(idx);
    }

    // 3. Steepest-descent trace per source: at each step, move to the lowest
    //    neighbour strictly below the current tile's height (ties broken by the
    //    lowest hex-direction index — pure and deterministic); stop at ocean, a
    //    local basin (no lower unvisited neighbour), or when the path would
    //    revisit a tile already on THIS trace (loop guard — merging into an
    //    earlier river's already-stamped tiles is fine; the new trace simply
    //    keeps descending through them).
    for (int src : sources)
    {
        std::vector<char> on_this_path(static_cast<std::size_t>(total), 0);
        int cur = src;
        for (int steps = 0; steps < kMaxRiverLength; ++steps)
        {
            if (is_ocean(cur))
                break;
            on_this_path[static_cast<std::size_t>(cur)] = 1;

            const int col = cur % gw, row = cur / gw;
            std::pair<int, int> nb[6];
            int cnt = 0;
            hex_neighbours(col, row, gw, gh, nb, cnt);

            int best = -1;
            int best_dir = -1;
            float best_height = height_at(cur);
            for (int i = 0; i < 6; ++i)
            {
                if (nb[i].first == -1)
                    continue;
                const int nidx = nb[i].first + nb[i].second * gw;
                if (on_this_path[static_cast<std::size_t>(nidx)])
                    continue;
                const float h = height_at(nidx);
                if (h < best_height)
                {
                    best_height = h;
                    best = nidx;
                    best_dir = i;
                }
            }
            if (best == -1)
                break; // local basin — no strictly-lower unvisited neighbour

            // Stamp the (cur, best) edge on BOTH tiles: bit best_dir on cur,
            // and its reciprocal opposite bit on best (guaranteed correct by
            // hex_neighbours' cube-direction construction — no per-edge lookup).
            if (tile_component* cur_tc = tile_at(cur))
                cur_tc->river_edges |= static_cast<std::uint8_t>(1u << best_dir);
            if (tile_component* nxt_tc = tile_at(best))
                nxt_tc->river_edges |= static_cast<std::uint8_t>(1u << opposite_direction(best_dir));

            cur = best;
        }
    }
}
