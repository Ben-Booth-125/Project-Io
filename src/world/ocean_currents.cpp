#include "world/ocean_currents.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <limits>
#include <queue>
#include <utility>

// ---------------------------------------------------------------------------
// Ocean currents (BL-1120). The header carries the model; this file is its
// arithmetic, integer throughout.
// ---------------------------------------------------------------------------

namespace
{

/// Floor square root of a non-negative 64-bit integer, by Newton's method on
/// integers only (no float, so no platform can round it differently).
int64_t isqrt64(int64_t n)
{
    if (n <= 0) return 0;
    int64_t x = n;
    int64_t y = (x + 1) / 2;
    while (y < x)
    {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

/// The band profile of the stream function at one row, per mille, before the
/// coast taper and before the rotation sense.
///
/// Signed latitude runs +1000 at row 0 (the north pole) to -1000 at the last
/// row, linear in the row — the same equirectangular reading Pass 3's
/// `band_for_row` takes (TILE_GENERATION.md § Pass 3). Its magnitude is cut
/// into three equal bands of 30 degrees, and inside each the profile is a
/// smoothstep S(t) = 3t^2 - 2t^3: rising 0 -> 1000 across the tropics,
/// falling back to 0 across the mid-latitudes, rising again to the pole. The
/// current is the profile's slope, so it is the smoothstep's derivative
/// 6t(1-t): zero at every band edge and strongest mid-band — easterly trades
/// peaking near 15 degrees, westerlies near 45, polar easterlies near 75 —
/// and signed so both hemispheres' trades blow west (the profile is odd in
/// latitude).
int band_profile_q(int row, int gh)
{
    if (gh <= 1) return 0;
    const int64_t span = gh - 1;
    const int64_t lat  = ((span - 2 * static_cast<int64_t>(row)) * 1000) / span; // symmetric: truncates toward 0
    const int64_t a    = lat < 0 ? -lat : lat;
    const int64_t band = std::min<int64_t>(2, (a * 3) / 1000);
    const int64_t t    = a * 3 - band * 1000; // 0..1000 inside the band
    const int64_t s    = (t * t * (3000 - 2 * t)) / 1000000; // smoothstep, 0..1000
    const int64_t psi  = (band == 1) ? (1000 - s) : s;
    return static_cast<int>(lat < 0 ? -psi : psi);
}

} // namespace

ocean_current_field build_ocean_currents(const std::vector<terrain_substrate>& substrate,
                                         int gw, int gh, int rotation_sense)
{
    ocean_current_field f;
    if (gw <= 0 || gh <= 1) return f;
    if (rotation_sense != 1 && rotation_sense != -1) return f;
    const std::size_t n = static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh);
    if (substrate.size() != n) return f;

    f.gw = gw;
    f.gh = gh;
    f.cell = ocean_current_cell_tiles;
    f.cells_w = (gw + f.cell - 1) / f.cell;
    f.cells_h = (gh + f.cell - 1) / f.cell;
    f.rotation_sense = rotation_sense;

    f.sea.assign(n, 0);
    for (std::size_t i = 0; i < n; ++i)
        f.sea[i] = is_sea(substrate[i]) ? 1 : 0;

    // ---- Distance to the nearest ground, capped at the taper -------------
    //
    // A multi-source breadth-first walk from every tile that is not sea (land
    // and lakes alike: a lake shore is ground to the ocean), eight-connected
    // so the distance is Chebyshev, columns wrapping and rows not. The walk's
    // ORDER cannot reach the answer: breadth-first distance is the unique
    // shortest-hop count, whichever equal-distance tile is expanded first.
    // A body with no ground at all leaves every tile at the cap — open water
    // everywhere, the band's wind unturned.
    const int R = ocean_current_coast_taper_tiles;
    std::vector<int> dist(n, R);
    std::vector<int> frontier;
    frontier.reserve(n);
    for (std::size_t i = 0; i < n; ++i)
        if (!f.sea[i]) { dist[i] = 0; frontier.push_back(static_cast<int>(i)); }
    for (int d = 0; d < R && !frontier.empty(); ++d)
    {
        std::vector<int> next;
        for (int idx : frontier)
        {
            const int c = idx % gw, r = idx / gw;
            for (int dr = -1; dr <= 1; ++dr)
            {
                const int rr = r + dr;
                if (rr < 0 || rr >= gh) continue;
                for (int dc = -1; dc <= 1; ++dc)
                {
                    if (dc == 0 && dr == 0) continue;
                    const int cc = ((c + dc) % gw + gw) % gw;
                    const std::size_t j = static_cast<std::size_t>(rr) * static_cast<std::size_t>(gw)
                                        + static_cast<std::size_t>(cc);
                    if (dist[j] > d + 1)
                    {
                        dist[j] = d + 1;
                        next.push_back(static_cast<int>(j));
                    }
                }
            }
        }
        frontier.swap(next);
    }

    // ---- The stream function, per tile ------------------------------------
    std::vector<int> profile(static_cast<std::size_t>(gh));
    for (int r = 0; r < gh; ++r)
        profile[static_cast<std::size_t>(r)] = band_profile_q(r, gh) * rotation_sense;
    std::vector<int> psi(n, 0);
    for (int r = 0; r < gh; ++r)
        for (int c = 0; c < gw; ++c)
        {
            const std::size_t i = static_cast<std::size_t>(r) * static_cast<std::size_t>(gw)
                                + static_cast<std::size_t>(c);
            if (!f.sea[i]) continue; // ground is psi = 0: every coast is a streamline
            psi[i] = (profile[static_cast<std::size_t>(r)] * std::min(dist[i], R)) / R;
        }

    // ---- The curl, summed into ocean regions ------------------------------
    //
    // east = d psi / d(south), north = d psi / d(east), by centred differences
    // (one-sided on the first and last rows, which have no row beyond them;
    // columns wrap). Each difference is carried DOUBLED — a centred step spans
    // two tiles, a one-sided one is doubled to match — so the per-tile value
    // stays an integer until the region's mean is taken, and scaled by R so a
    // boundary current at the band's full profile reads 1000.
    const std::size_t cells = static_cast<std::size_t>(f.cells_w) * static_cast<std::size_t>(f.cells_h);
    std::vector<int64_t> sum_e(cells, 0), sum_n(cells, 0);
    f.sea_tiles.assign(cells, 0);
    const auto at = [&](int c, int r) {
        return psi[static_cast<std::size_t>(r) * static_cast<std::size_t>(gw)
                   + static_cast<std::size_t>(((c % gw) + gw) % gw)];
    };
    for (int r = 0; r < gh; ++r)
        for (int c = 0; c < gw; ++c)
        {
            const std::size_t i = static_cast<std::size_t>(r) * static_cast<std::size_t>(gw)
                                + static_cast<std::size_t>(c);
            if (!f.sea[i]) continue;
            int64_t e2;
            if (r == 0)            e2 = 2 * static_cast<int64_t>(at(c, 1) - at(c, 0));
            else if (r == gh - 1)  e2 = 2 * static_cast<int64_t>(at(c, r) - at(c, r - 1));
            else                   e2 = static_cast<int64_t>(at(c, r + 1) - at(c, r - 1));
            const int64_t n2 = static_cast<int64_t>(at(c + 1, r) - at(c - 1, r));
            const std::size_t k = static_cast<std::size_t>(f.region_of(c, r));
            sum_e[k] += e2 * R;
            sum_n[k] += n2 * R;
            ++f.sea_tiles[k];
        }
    f.east_q.assign(cells, 0);
    f.north_q.assign(cells, 0);
    for (std::size_t k = 0; k < cells; ++k)
    {
        if (f.sea_tiles[k] == 0) continue;
        const int64_t den = 2 * static_cast<int64_t>(f.sea_tiles[k]);
        f.east_q[k]  = static_cast<int16_t>(std::clamp<int64_t>(sum_e[k] / den, -1000, 1000));
        f.north_q[k] = static_cast<int16_t>(std::clamp<int64_t>(sum_n[k] / den, -1000, 1000));
    }
    return f;
}

int ocean_current_alignment_q(const ocean_current_field& f,
                              int a_col, int a_row, int b_col, int b_row)
{
    if (f.empty()) return 0;
    if (a_col < 0 || a_row < 0 || a_col >= f.gw || a_row >= f.gh) return 0;
    if (b_col < 0 || b_row < 0 || b_col >= f.gw || b_row >= f.gh) return 0;
    const int ia = a_row * f.gw + a_col;
    const int ib = b_row * f.gw + b_col;
    if (ia == ib) return 0;

    // THE CANONICAL WALK: always from the lower raster index, so both
    // directions of one leg sample the identical tiles and the return is the
    // exact negation of the outbound (integer truncation is symmetric about
    // zero, so negating the direction negates every term).
    const bool flip = ib < ia;
    const int c0 = flip ? b_col : a_col, r0 = flip ? b_row : a_row;
    const int c1 = flip ? a_col : b_col, r1 = flip ? a_row : b_row;
    int dc = c1 - c0;
    const int half = f.gw / 2;
    if (dc > half)       dc -= f.gw; // the short way round the cylinder
    else if (dc < -half) dc += f.gw;
    const int dr = r1 - r0;
    const int steps = std::max(dc < 0 ? -dc : dc, dr < 0 ? -dr : dr);
    if (steps == 0) return 0;
    const int64_t len = isqrt64(static_cast<int64_t>(dc) * dc + static_cast<int64_t>(dr) * dr);
    if (len <= 0) return 0;

    int64_t sum = 0;
    for (int t = 0; t <= steps; ++t)
    {
        const int c = (((c0 + (dc * t) / steps) % f.gw) + f.gw) % f.gw;
        const int r = r0 + (dr * t) / steps;
        const std::size_t idx = static_cast<std::size_t>(r) * static_cast<std::size_t>(f.gw)
                              + static_cast<std::size_t>(c);
        if (!f.sea[idx]) continue; // ground carries no current
        const std::size_t k = static_cast<std::size_t>(f.region_of(c, r));
        // Direction east = dc, north = -dr (rows run south).
        const int64_t dot = static_cast<int64_t>(f.east_q[k]) * dc
                          + static_cast<int64_t>(f.north_q[k]) * (-dr);
        sum += dot / len;
    }
    const int align = static_cast<int>(std::clamp<int64_t>(sum / (steps + 1), -1000, 1000));
    return flip ? -align : align;
}

std::vector<int32_t> landmass_labels(const std::vector<terrain_substrate>& substrate, int gw, int gh)
{
    std::vector<int32_t> mass(substrate.size(), -1);
    if (gw <= 0 || gh <= 0
     || substrate.size() != static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh))
        return mass;
    // A depth-first fill from each unlabelled non-sea tile, walked in raster
    // order: the components are a property of the raster, and the numbering
    // is the raster order of each component's first tile.
    int32_t next = 0;
    std::vector<int> stack;
    for (int start = 0; start < gw * gh; ++start)
    {
        if (mass[static_cast<std::size_t>(start)] >= 0 || is_sea(substrate[static_cast<std::size_t>(start)]))
            continue;
        mass[static_cast<std::size_t>(start)] = next;
        stack.push_back(start);
        while (!stack.empty())
        {
            const int idx = stack.back();
            stack.pop_back();
            const int c = idx % gw, r = idx / gw;
            // Four cardinal neighbours, columns wrapping: the grid every traversal
            // reader walks (logistics.cpp), so two landmasses are two exactly when
            // no four-way walk over ground joins them.
            const int nb[4][2] = { {(c + 1) % gw, r}, {(c + gw - 1) % gw, r}, {c, r + 1}, {c, r - 1} };
            for (const auto& n : nb)
            {
                if (n[1] < 0 || n[1] >= gh) continue;
                const int ni = n[1] * gw + n[0];
                if (mass[static_cast<std::size_t>(ni)] >= 0 || is_sea(substrate[static_cast<std::size_t>(ni)]))
                    continue;
                mass[static_cast<std::size_t>(ni)] = next;
                stack.push_back(ni);
            }
        }
        ++next;
    }
    return mass;
}

int32_t landmass_at(const std::vector<int32_t>& labels, int gw, int gh, int col, int row)
{
    if (gw <= 0 || gh <= 0 || col < 0 || row < 0 || col >= gw || row >= gh) return -1;
    if (labels.size() != static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh)) return -1;
    const int32_t here = labels[static_cast<std::size_t>(row) * static_cast<std::size_t>(gw)
                                + static_cast<std::size_t>(col)];
    if (here >= 0) return here;
    // (label, count) over the 5x5 window: at most 24 distinct land labels (the
    // centre is sea), held in a fixed array -- no allocation per read.
    int32_t seen_label[25];
    int     seen_count[25];
    int     seen_n = 0;
    for (int dr = -2; dr <= 2; ++dr)
        for (int dc = -2; dc <= 2; ++dc)
        {
            const int rr = row + dr;
            if (rr < 0 || rr >= gh) continue;
            const int cc = ((col + dc) % gw + gw) % gw;
            const int32_t m = labels[static_cast<std::size_t>(rr) * static_cast<std::size_t>(gw)
                                     + static_cast<std::size_t>(cc)];
            if (m < 0) continue;
            int k = 0;
            while (k < seen_n && seen_label[k] != m) ++k;
            if (k < seen_n) ++seen_count[k];
            else { seen_label[seen_n] = m; seen_count[seen_n] = 1; ++seen_n; }
        }
    int32_t best = -1;
    int     best_n = 0;
    for (int k = 0; k < seen_n; ++k)
        if (seen_count[k] > best_n || (seen_count[k] == best_n && seen_label[k] < best))
        { best = seen_label[k]; best_n = seen_count[k]; }
    return best;
}

int nearest_sea_tile(const std::vector<std::uint8_t>& sea, int gw, int gh, int col, int row, int radius)
{
    if (gw <= 0 || gh <= 0 || sea.size() != static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh))
        return -1;
    if (col < 0 || row < 0 || col >= gw || row >= gh)
        return -1;
    // Ring by ring outward, so the first ring holding any sea is the nearest; inside
    // it the lowest raster index wins, which no scan order can change.
    for (int r = 0; r <= radius; ++r)
    {
        int best = -1;
        for (int dr = -r; dr <= r; ++dr)
        {
            const int rr = row + dr;
            if (rr < 0 || rr >= gh)
                continue;
            for (int dc = -r; dc <= r; ++dc)
            {
                if (std::max(std::abs(dc), std::abs(dr)) != r)
                    continue; // the ring's rim only
                const int cc = ((col + dc) % gw + gw) % gw;
                const int idx = rr * gw + cc;
                if (sea[static_cast<std::size_t>(idx)] && (best < 0 || idx < best))
                    best = idx;
            }
        }
        if (best >= 0)
            return best;
    }
    return -1;
}

namespace
{

/// The walk's Dijkstra, shared by `sea_walk` (one source, stop at @p to) and
/// `sea_cost_field` (many sources, no stop: @p to = -1). Fills @p dist and,
/// when given, @p prev.
void sea_dijkstra(const std::vector<std::uint8_t>& sea, int gw, int gh,
                  const ocean_current_field* currents, int weight_q,
                  const std::vector<int>& sources, int to,
                  std::vector<int64_t>& dist, std::vector<int>* prev,
                  const std::vector<std::uint8_t>* laned = nullptr, int laned_cost_q = 1000)
{
    const int n = gw * gh;
    constexpr int64_t kInf = std::numeric_limits<int64_t>::max();
    dist.assign(static_cast<std::size_t>(n), kInf);
    if (prev != nullptr) prev->assign(static_cast<std::size_t>(n), -1);
    const bool priced = currents != nullptr && !currents->empty() && weight_q > 0
                     && currents->gw == gw && currents->gh == gh;
    // A laned tile is entered at laned_cost_q per mille of its priced step; 1000
    // (or no mask) is the plain walk, byte for byte.
    const bool reuse = laned != nullptr && laned->size() == static_cast<std::size_t>(n)
                    && laned_cost_q >= 0 && laned_cost_q < 1000;
    using node = std::pair<int64_t, int>;
    std::priority_queue<node, std::vector<node>, std::greater<node>> frontier;
    for (const int s : sources)
    {
        if (s < 0 || s >= n || !sea[static_cast<std::size_t>(s)]) continue;
        if (dist[static_cast<std::size_t>(s)] == 0) continue;
        dist[static_cast<std::size_t>(s)] = 0;
        frontier.push({0, s});
    }
    while (!frontier.empty())
    {
        const node top = frontier.top();
        frontier.pop();
        const int u = top.second;
        if (top.first > dist[static_cast<std::size_t>(u)])
            continue; // a superseded entry
        if (u == to)
            break;
        const int uc = u % gw, ur = u / gw;
        // FOUR CARDINAL STEPS, columns wrapping and rows not -- the pathfinder's own
        // grid (logistics.cpp, LOGISTICS.md sec 2). A lane walked eight ways leaves
        // no two laned tiles side by side on a diagonal, so a four-way traveller
        // rides it at about x0.75 instead of x0.50; walked on the traveller's grid,
        // every step of the lane is a step the traveller can take.
        constexpr int kStepDc[4] = { 0, 0, -1, 1 };
        constexpr int kStepDr[4] = { -1, 1, 0, 0 };
        for (int k4 = 0; k4 < 4; ++k4)
        {
            const int dc = kStepDc[k4], dr = kStepDr[k4];
            const int vr = ur + dr;
            if (vr < 0 || vr >= gh)
                continue; // rows do not wrap
            const int vc = ((uc + dc) % gw + gw) % gw;
            const int v = vr * gw + vc;
            if (!sea[static_cast<std::size_t>(v)])
                continue; // water only
            constexpr int64_t len = 1000;
            int64_t step = len;
            if (priced)
            {
                // The step's direction (east = dc, north = -dr) against the entered
                // tile's ocean-region current, per mille of a full current along it.
                const std::size_t k = static_cast<std::size_t>(currents->region_of(vc, vr));
                const int64_t dot = static_cast<int64_t>(currents->east_q[k]) * dc
                                  + static_cast<int64_t>(currents->north_q[k]) * (-dr);
                const int align = static_cast<int>(std::clamp<int64_t>(dot, -1000, 1000));
                step = (len * ocean_current_leg_cost_q(weight_q, align)) / 1000;
                if (step < 1)
                    step = 1; // no step is ever free
            }
            if (reuse && (*laned)[static_cast<std::size_t>(v)])
            {
                step = (step * laned_cost_q) / 1000;
                if (step < 1)
                    step = 1;
            }
            const int64_t cand = dist[static_cast<std::size_t>(u)] + step;
            if (cand < dist[static_cast<std::size_t>(v)])
            {
                dist[static_cast<std::size_t>(v)] = cand;
                if (prev != nullptr) (*prev)[static_cast<std::size_t>(v)] = u;
                frontier.push({cand, v});
            }
        }
    }
}

} // namespace

std::vector<int> sea_walk(const std::vector<std::uint8_t>& sea, int gw, int gh,
                          const ocean_current_field* currents, int weight_q, int from, int to)
{
    return sea_walk_laned(sea, gw, gh, currents, weight_q, nullptr, 1000, from, to);
}

std::vector<int> sea_walk_laned(const std::vector<std::uint8_t>& sea, int gw, int gh,
                                const ocean_current_field* currents, int weight_q,
                                const std::vector<std::uint8_t>* laned, int laned_cost_q, int from, int to)
{
    std::vector<int> path;
    const int n = gw * gh;
    if (gw <= 0 || gh <= 0 || sea.size() != static_cast<std::size_t>(n))
        return path;
    if (from < 0 || to < 0 || from >= n || to >= n || !sea[static_cast<std::size_t>(from)]
        || !sea[static_cast<std::size_t>(to)])
        return path;
    if (from == to)
    {
        path.push_back(from);
        return path;
    }
    std::vector<int64_t> dist;
    std::vector<int>     prev;
    sea_dijkstra(sea, gw, gh, currents, weight_q, { from }, to, dist, &prev, laned, laned_cost_q);
    if (dist[static_cast<std::size_t>(to)] == std::numeric_limits<int64_t>::max())
        return path;
    for (int t = to; t >= 0; t = prev[static_cast<std::size_t>(t)])
    {
        path.push_back(t);
        if (t == from)
            break;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<int> sea_tiles_by_ring(const std::vector<std::uint8_t>& sea, int gw, int gh, int col, int row,
                                   int radius)
{
    std::vector<int> out;
    if (gw <= 0 || gh <= 0 || sea.size() != static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh))
        return out;
    if (col < 0 || row < 0 || col >= gw || row >= gh)
        return out;
    for (int r = 0; r <= radius; ++r)
    {
        std::vector<int> ring;
        for (int dr = -r; dr <= r; ++dr)
        {
            const int rr = row + dr;
            if (rr < 0 || rr >= gh)
                continue;
            for (int dc = -r; dc <= r; ++dc)
            {
                if (std::max(std::abs(dc), std::abs(dr)) != r)
                    continue;
                const int cc = ((col + dc) % gw + gw) % gw;
                const int idx = rr * gw + cc;
                if (sea[static_cast<std::size_t>(idx)])
                    ring.push_back(idx);
            }
        }
        std::sort(ring.begin(), ring.end());
        ring.erase(std::unique(ring.begin(), ring.end()), ring.end()); // a wrapped ring can meet itself
        for (const int t : ring)
            if (std::find(out.begin(), out.end(), t) == out.end())
                out.push_back(t);
    }
    return out;
}

std::vector<int> sea_components(const std::vector<std::uint8_t>& sea, int gw, int gh)
{
    std::vector<int> comp;
    const int n = gw * gh;
    if (gw <= 0 || gh <= 0 || sea.size() != static_cast<std::size_t>(n))
        return comp;
    comp.assign(static_cast<std::size_t>(n), -1);
    int label = 0;
    std::vector<int> stack;
    for (int s = 0; s < n; ++s)
    {
        if (!sea[static_cast<std::size_t>(s)] || comp[static_cast<std::size_t>(s)] >= 0)
            continue;
        comp[static_cast<std::size_t>(s)] = label;
        stack.assign(1, s);
        while (!stack.empty())
        {
            const int u = stack.back();
            stack.pop_back();
            const int uc = u % gw, ur = u / gw;
            constexpr int kDc[4] = { 0, 0, -1, 1 };
            constexpr int kDr[4] = { -1, 1, 0, 0 };
            for (int k = 0; k < 4; ++k)
            {
                const int vr = ur + kDr[k];
                if (vr < 0 || vr >= gh)
                    continue;
                const int vc = ((uc + kDc[k]) % gw + gw) % gw;
                const int v = vr * gw + vc;
                if (!sea[static_cast<std::size_t>(v)] || comp[static_cast<std::size_t>(v)] >= 0)
                    continue;
                comp[static_cast<std::size_t>(v)] = label;
                stack.push_back(v);
            }
        }
        ++label;
    }
    return comp;
}

sea_field sea_field_from(const std::vector<std::uint8_t>& sea, int gw, int gh,
                         const ocean_current_field* currents, int weight_q, int source)
{
    sea_field f;
    if (gw <= 0 || gh <= 0 || sea.size() != static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh))
        return f;
    sea_dijkstra(sea, gw, gh, currents, weight_q, { source }, -1, f.dist, &f.prev);
    return f;
}

std::vector<int> sea_path_on(const sea_field& f, int to)
{
    std::vector<int> path;
    if (to < 0 || static_cast<std::size_t>(to) >= f.dist.size()
        || f.dist[static_cast<std::size_t>(to)] == std::numeric_limits<int64_t>::max())
        return path;
    for (int t = to; t >= 0; t = f.prev[static_cast<std::size_t>(t)])
        path.push_back(t);
    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<int64_t> sea_cost_field(const std::vector<std::uint8_t>& sea, int gw, int gh,
                                    const ocean_current_field* currents, int weight_q,
                                    const std::vector<int>& sources)
{
    std::vector<int64_t> dist;
    if (gw <= 0 || gh <= 0 || sea.size() != static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh))
        return dist;
    sea_dijkstra(sea, gw, gh, currents, weight_q, sources, -1, dist, nullptr);
    return dist;
}

int64_t fleet_power_at(int64_t navy, int64_t cost, int64_t halving_tiles)
{
    if (navy <= 0 || halving_tiles <= 0 || cost < 0 || cost == std::numeric_limits<int64_t>::max())
        return 0;
    const int64_t half = halving_tiles * 1000; // cost units per halving (still water)
    const int64_t k = cost / half;
    if (k >= 40) return 0;
    const int64_t r = cost % half;
    const int64_t base = std::min<int64_t>(navy, int64_t{1} << 40) * 1024;
    const int64_t p = base >> k;
    // Linear within the halving: from p at its start to p/2 at its end. The
    // product p * r can pass int64 inside the domain (p < 2^51, r < 10^8), so
    // floor(p * r / D) is taken as (p / D) * r + floor((p % D) * r / D), exact
    // and in range: p % D < D <= 2 x 10^8, so (p % D) * r < 2 x 10^16.
    const int64_t D = 2 * half;
    return p - ((p / D) * r + ((p % D) * r) / D);
}

uint64_t ocean_current_digest(const ocean_current_field& f)
{
    uint64_t h = 1469598103934665603ULL;
    const auto mix = [&](int64_t v) {
        for (int b = 0; b < 8; ++b)
        {
            h ^= static_cast<uint64_t>((v >> (8 * b)) & 0xFF);
            h *= 1099511628211ULL;
        }
    };
    mix(f.gw); mix(f.gh); mix(f.cell); mix(f.rotation_sense);
    for (std::size_t k = 0; k < f.east_q.size(); ++k)
    {
        mix(f.east_q[k]);
        mix(f.north_q[k]);
        mix(f.sea_tiles[k]);
    }
    return h;
}
