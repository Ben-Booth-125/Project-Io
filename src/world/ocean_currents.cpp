#include "world/ocean_currents.hpp"

#include <algorithm>
#include <cstddef>

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
