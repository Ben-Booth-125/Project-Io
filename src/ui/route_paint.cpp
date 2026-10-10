#include "route_paint.hpp"

#include "ground_bake.hpp"
#include "terrain_palette.hpp"
#include "world/hex_neighbors.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

// BL-1253 (roads painted). See route_paint.hpp and RENDERING.md § Roads and
// sea lanes. Everything here is a pure function of the bake source: hashes key
// on WRAPPED grid coordinates, positions are tile-relative, so every wrap copy
// and every chunk paints a route identically.

namespace ui::ground {

namespace {

constexpr double kSqrt3 = 1.7320508075688772;
constexpr double kPi    = 3.14159265358979323846;

/// The same FNV-style lattice hash the rest of the bake uses (a local copy:
/// this TU keeps the route pass self-contained).
float rh01(int x, int y, std::uint32_t salt)
{
    std::uint32_t h = 2166136261u;
    auto mix = [&h](std::uint32_t v) { h ^= v; h *= 16777619u; h ^= h >> 13; };
    mix(static_cast<std::uint32_t>(x) * 73856093u);
    mix(static_cast<std::uint32_t>(y) * 19349663u);
    mix(salt * 83492791u);
    h ^= h >> 16;
    return static_cast<float>(h & 0x00FFFFFFu) / 16777216.0f;
}

/// Bilinear value noise, smoothstepped, wrap-periodic in x (@p period_cells
/// lattice cells per wrap).
float rnoise(double x, double y, double cell, int period_cells, std::uint32_t salt)
{
    const double fx = x / cell, fy = y / cell;
    const int ix = static_cast<int>(std::floor(fx));
    const int iy = static_cast<int>(std::floor(fy));
    float tx = static_cast<float>(fx - ix), ty = static_cast<float>(fy - iy);
    tx = tx * tx * (3.0f - 2.0f * tx);
    ty = ty * ty * (3.0f - 2.0f * ty);
    auto wrap = [&](int v) { return ((v % period_cells) + period_cells) % period_cells; };
    const float v00 = rh01(wrap(ix), iy, salt),     v10 = rh01(wrap(ix + 1), iy, salt);
    const float v01 = rh01(wrap(ix), iy + 1, salt), v11 = rh01(wrap(ix + 1), iy + 1, salt);
    const float a = v00 + (v10 - v00) * tx, b = v01 + (v11 - v01) * tx;
    return a + (b - a) * ty;
}

/// One lattice cell's hash, wrap-periodic in x — a speckle of cell @p cell.
float rspeck(double x, double y, double cell, int period_cells, std::uint32_t salt)
{
    const int ix = static_cast<int>(std::floor(x / cell));
    const int iy = static_cast<int>(std::floor(y / cell));
    return rh01(((ix % period_cells) + period_cells) % period_cells, iy, salt);
}

inline double smooth01(double e0, double e1, double x)
{
    const double t = std::clamp((x - e0) / (e1 - e0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

/// Centre-to-centre offset across hex side i (0=E, 1=NE, 2=NW, 3=W, 4=SW,
/// 5=SE): row-parity independent (hex_neighbors' side table).
constexpr double kNbDx[6] = { kSqrt3, kSqrt3 * 0.5, -kSqrt3 * 0.5, -kSqrt3, -kSqrt3 * 0.5, kSqrt3 * 0.5 };
constexpr double kNbDy[6] = { 0.0, -1.5, -1.5, 0.0, 1.5, 1.5 };

inline int nb_index(const bake_source& s, int i, int side)
{
    const int r = i / s.gw, c = i % s.gw;
    const hex_neighbors::coord nb = hex_neighbors::neighbour(c, r, side);
    if (nb.gy < 0 || nb.gy >= s.gh)
        return -1;
    const int cw = ((nb.gx % s.gw) + s.gw) % s.gw;
    return nb.gy * s.gw + cw;
}

inline bool has_routes(const bake_source& s)
{
    const std::size_t n = static_cast<std::size_t>(s.gw) * s.gh;
    return n > 0 && s.road.size() == n && s.lane.size() == n && s.route_links.size() == n
        && s.route_first.size() == n && s.route_count.size() == n && s.near_route.size() == n;
}

// ---------------------------------------------------------------------------
// The cardinal links — the canvas's four directions, in its order: E, W, S, N
// (`card_off`). E/W are the row neighbours; S/N the same COLUMN one row down /
// up, which on the odd-r grid is the SE or SW (NE or NW) hex by row parity —
// a true hex neighbour, so the centre-to-centre midpoint is the shared edge's.
// ---------------------------------------------------------------------------

constexpr int k_card[4][2] = { { +1, 0 }, { -1, 0 }, { 0, +1 }, { 0, -1 } };

inline void link_mid(int r, int n, double& mx, double& my)
{
    const double sx = (r & 1) ? -kSqrt3 * 0.25 : kSqrt3 * 0.25;
    switch (n)
    {
        case 0:  mx =  kSqrt3 * 0.5; my =  0.0;  break;
        case 1:  mx = -kSqrt3 * 0.5; my =  0.0;  break;
        case 2:  mx = sx;            my =  0.75; break;
        default: mx = sx;            my = -0.75; break;
    }
}

inline bool on_net(const std::vector<std::uint8_t>& lv, const bake_source& s, int c, int r)
{
    if (r < 0 || r >= s.gh)
        return false;
    const int cw = ((c % s.gw) + s.gw) % s.gw;
    return lv[static_cast<std::size_t>(r) * s.gw + cw] != 0;
}

/// Link bits (E, W, S, N = bits 0-3) of tile (c, r) on network @p lv. Lanes
/// skip RUNGS: a link between two tiles that both run straight through along
/// the other axis (the canvas rule, symmetric so the halves still meet).
std::uint8_t links_of(const std::vector<std::uint8_t>& lv, const bake_source& s, int c, int r,
                      bool skip_rungs)
{
    const auto through = [&](int cc, int rr, bool horizontal) {
        return horizontal ? (on_net(lv, s, cc + 1, rr) && on_net(lv, s, cc - 1, rr))
                          : (on_net(lv, s, cc, rr + 1) && on_net(lv, s, cc, rr - 1));
    };
    std::uint8_t bits = 0;
    for (int n = 0; n < 4; ++n)
    {
        const int nc = c + k_card[n][0], nr = r + k_card[n][1];
        if (!on_net(lv, s, nc, nr))
            continue;
        if (skip_rungs)
        {
            const bool other_axis_horizontal = n >= 2;
            if (through(c, r, other_axis_horizontal) && through(nc, nr, other_axis_horizontal))
                continue;
        }
        bits |= static_cast<std::uint8_t>(1u << n);
    }
    return bits;
}

/// Painted half-width and the reach beyond it (verge, ditch, shoulder, wake).
inline double tier_hw(std::uint8_t tier)
{
    if (tier & 0x80u)
        return 0.5 * k_lane_paint_width;
    return 0.5 * k_route_width[std::min<int>(tier, k_route_rail)];
}
inline double tier_out(std::uint8_t tier)
{
    if (tier & 0x80u)
        return 0.035; // the wake's spread past the lane's width
    switch (tier)
    {
        case k_route_track:   return 0.020; // the worn margin
        case k_route_road:    return 0.044; // verge, then the ditch
        case k_route_highway: return 0.026; // the shoulder
        default:              return 0.018; // ballast spill
    }
}

void finish_piece(route_piece& pc)
{
    pc.cum[0] = 0.0f;
    pc.bx0 = pc.bx1 = pc.x[0];
    pc.by0 = pc.by1 = pc.y[0];
    for (int k = 1; k < route_piece::k_pts; ++k)
    {
        const double dx = pc.x[k] - pc.x[k - 1], dy = pc.y[k] - pc.y[k - 1];
        pc.cum[k] = pc.cum[k - 1] + static_cast<float>(std::sqrt(dx * dx + dy * dy));
        pc.bx0 = std::min(pc.bx0, pc.x[k]); pc.bx1 = std::max(pc.bx1, pc.x[k]);
        pc.by0 = std::min(pc.by0, pc.y[k]); pc.by1 = std::max(pc.by1, pc.y[k]);
    }
    pc.split = pc.kind == route_piece::curve ? pc.cum[route_piece::k_pts / 2] : 0.0f;
}

/// A straight piece from (ax, ay) to (bx, by): the far end (b) is the shared
/// edge's midpoint, where a dash pattern's phase is pinned.
route_piece spoke(double ax, double ay, double bx, double by, std::uint8_t tier, std::uint8_t kind)
{
    route_piece pc;
    pc.tier = tier;
    pc.kind = kind;
    for (int k = 0; k < route_piece::k_pts; ++k)
    {
        const double t = static_cast<double>(k) / (route_piece::k_pts - 1);
        pc.x[k] = static_cast<float>(ax + (bx - ax) * t);
        pc.y[k] = static_cast<float>(ay + (by - ay) * t);
    }
    finish_piece(pc);
    return pc;
}

/// The quadratic m_a -> (0, 0) -> m_b, bowed by (bwx, bwy) * sin^2(pi t): the
/// bow and its derivative vanish at both ends, so a bowed curve still meets
/// its neighbours tangent-continuous at the shared midpoints.
route_piece curve(double ax, double ay, double bx, double by, double bwx, double bwy,
                  std::uint8_t tier)
{
    route_piece pc;
    pc.tier = tier;
    pc.kind = route_piece::curve;
    for (int k = 0; k < route_piece::k_pts; ++k)
    {
        const double t = static_cast<double>(k) / (route_piece::k_pts - 1);
        const double a = (1.0 - t) * (1.0 - t), c = t * t;
        const double sn = std::sin(kPi * t);
        const double w = sn * sn;
        pc.x[k] = static_cast<float>(a * ax + c * bx + w * bwx);
        pc.y[k] = static_cast<float>(a * ay + c * by + w * bwy);
    }
    finish_piece(pc);
    return pc;
}

/// The pieces of tile @p i on one network (the canvas's draw_network, ported):
/// through-curves paired most-opposite first, an odd branch or an end a spoke,
/// a lone tile a yard. On a roaded built tile (@p plan) a through-curve bows
/// away from the cluster, an arriving road ends at the forecourt apron, and a
/// short spur joins a through-road to it.
void tile_pieces(const bake_source& s, std::size_t i, std::uint8_t links, std::uint8_t tier,
                 const road_plan* plan, std::vector<route_piece>& out)
{
    const int r = static_cast<int>(i) / s.gw;
    double mx[4], my[4];
    int    deg = 0;
    for (int n = 0; n < 4; ++n)
        if (links & (1u << n))
        {
            link_mid(r, n, mx[deg], my[deg]);
            ++deg;
        }
    const double hw = tier_hw(tier);
    const bool roaded = plan && plan->roaded;
    const std::size_t mine = out.size(); // this tile's pieces start here

    if (deg == 0)
    {
        // A lone tile: a small yard where the canvas drew a cap. Nothing on a
        // tile that stands something: its own forms carry the ground there.
        const bool built = s.inst.of_tile.size() == s.cls.size() && s.inst.of_tile[i] >= 0;
        if (!built)
        {
            route_piece pc = spoke(0.0, 0.0, 0.0, 0.0, tier, route_piece::yard);
            out.push_back(pc);
        }
        return;
    }

    bool   used[4] = { false, false, false, false };
    double hubx = 0.0, huby = 0.0;
    bool   have_curve = false;
    for (int left = deg; left >= 2; left -= 2)
    {
        int pa = -1, pb = -1;
        double best = 2.0;
        for (int a = 0; a < deg; ++a)
        {
            if (used[a]) continue;
            for (int b = a + 1; b < deg; ++b)
            {
                if (used[b]) continue;
                const double la = std::sqrt(mx[a] * mx[a] + my[a] * my[a]);
                const double lb = std::sqrt(mx[b] * mx[b] + my[b] * my[b]);
                double d = (mx[a] * mx[b] + my[a] * my[b]) / (la * lb);
                d = std::round(d * 1e6) / 1e6; // exact ties keep the first pair, on any compiler
                if (d < best) { best = d; pa = a; pb = b; }
            }
        }
        if (pa < 0)
            break;
        used[pa] = used[pb] = true;
        double bwx = 0.0, bwy = 0.0;
        if (roaded)
        {
            // Bow away from the cluster until the road and its verges clear
            // its keep-out disc along the middle of the curve.
            const double clr = plan->radius + hw + tier_out(tier) + 0.02;
            for (double beta = 0.0; beta <= 0.6001; beta += 0.02)
            {
                bwx = -plan->fx * beta;
                bwy = -plan->fy * beta;
                bool clear = true;
                for (int k = 1; k < 16 && clear; ++k)
                {
                    const double t = k / 16.0;
                    const double a = (1.0 - t) * (1.0 - t), c = t * t;
                    const double sn = std::sin(kPi * t);
                    const double px = a * mx[pa] + c * mx[pb] + sn * sn * bwx;
                    const double py = a * my[pa] + c * my[pb] + sn * sn * bwy;
                    if (std::hypot(px - plan->kx, py - plan->ky) < clr)
                        clear = false;
                }
                if (clear)
                    break;
            }
        }
        out.push_back(curve(mx[pa], my[pa], mx[pb], my[pb], bwx, bwy, tier));
        // The hub an odd branch joins: this curve's apex.
        hubx = (mx[pa] + mx[pb]) * 0.25 + bwx;
        huby = (my[pa] + my[pb]) * 0.25 + bwy;
        have_curve = true;
    }
    for (int a = 0; a < deg; ++a)
        if (!used[a])
        {
            if (roaded && !have_curve)
                out.push_back(spoke(plan->apx, plan->apy, mx[a], my[a], tier, route_piece::spoke)); // ends at the forecourt
            else
                out.push_back(spoke(hubx, huby, mx[a], my[a], tier, route_piece::spoke));
        }
    if (roaded && have_curve)
    {
        // The access spur: from the through-road's nearest sample to the
        // forecourt, when the apron does not already touch the road.
        double bx = 0.0, by = 0.0, bd = 1e9;
        for (std::size_t q = mine; q < out.size(); ++q)
            for (int k = 0; k < route_piece::k_pts; ++k)
            {
                const route_piece& pc = out[q];
                const double d = std::hypot(pc.x[k] - plan->apx, pc.y[k] - plan->apy);
                if (d < bd) { bd = d; bx = pc.x[k]; by = pc.y[k]; }
            }
        if (bd > hw * 1.5)
            out.push_back(spoke(plan->apx, plan->apy, bx, by, tier, route_piece::spur));
    }
}

/// The nearest tile centre to canonical point (x, y): unwrapped column @p c,
/// row @p r, and its raster index (or -1 off the grid).
int nearest_tile(const bake_source& s, double x, double y, int& c_out, int& r_out)
{
    const int rr = static_cast<int>(std::floor(y / 1.5 + 0.5));
    double best = 1e30;
    int bi = -1;
    for (int r = rr - 1; r <= rr + 1; ++r)
    {
        if (r < 0 || r >= s.gh)
            continue;
        const double odd = (r & 1) ? 0.5 : 0.0;
        const int c = static_cast<int>(std::floor(x / kSqrt3 - odd + 0.5));
        const double dx = x - kSqrt3 * (c + odd), dy = y - 1.5 * r;
        const double d2 = dx * dx + dy * dy;
        if (d2 < best)
        {
            best = d2;
            c_out = c;
            r_out = r;
            bi = r * s.gw + ((c % s.gw) + s.gw) % s.gw;
        }
    }
    return bi;
}

/// The nearest piece to a point, and how many pieces' surfaces cover it.
struct route_hit
{
    double e     = 1e9;   ///< d - hw of the chosen piece (negative: on its surface).
    double d     = 1e9;   ///< Lateral distance to its centreline.
    double hw    = 0.0;
    double along = 0.0;   ///< Arclength from the piece's nearer pinned end.
    double half  = 1.0;   ///< Length of that half (dash counts are whole per half).
    std::uint8_t tier = 0, kind = 0;
    int    covers = 0;    ///< Pieces whose surface covers the point (>= 2: a junction).
    int    tile = -1;
};

/// Walk the pieces of the point's nearest tile and its six neighbours on one
/// network (@p lanes) — every piece that can reach a point lies on one of them.
/// @p slack widens the bounding-box cull: the paint needs only what can
/// touch the point; a clearance query wants the distance from farther out.
void query(const bake_source& s, double x, double y, bool lanes, route_hit& h,
           int* t0_out = nullptr, double* cx_out = nullptr, double slack = 0.0)
{
    int c0 = 0, r0 = 0;
    const int t0 = nearest_tile(s, x, y, c0, r0);
    if (t0_out) *t0_out = t0;
    if (t0 < 0)
        return;
    const double cx0 = kSqrt3 * (c0 + ((r0 & 1) ? 0.5 : 0.0)), cy0 = 1.5 * r0;
    if (cx_out) *cx_out = cx0;
    if (!(s.near_route[static_cast<std::size_t>(t0)] & (lanes ? 2u : 1u)))
        return;
    for (int k = -1; k < 6; ++k)
    {
        const int t = k < 0 ? t0 : nb_index(s, t0, k);
        if (t < 0)
            continue;
        const int n = s.route_count[static_cast<std::size_t>(t)];
        if (n == 0)
            continue;
        const double qx = x - cx0 - (k < 0 ? 0.0 : kNbDx[k]);
        const double qy = y - cy0 - (k < 0 ? 0.0 : kNbDy[k]);
        const std::size_t first = static_cast<std::size_t>(s.route_first[static_cast<std::size_t>(t)]);
        for (int j = 0; j < n; ++j)
        {
            const route_piece& pc = s.route_pieces[first + static_cast<std::size_t>(j)];
            if (((pc.tier & 0x80u) != 0) != lanes)
                continue;
            double hw = tier_hw(pc.tier);
            if (pc.kind == route_piece::yard)
                hw *= 1.5;
            const double reach = hw + tier_out(pc.tier) + slack;
            if (qx < pc.bx0 - reach || qx > pc.bx1 + reach || qy < pc.by0 - reach || qy > pc.by1 + reach)
                continue;
            double bd2 = 1e30, bs = 0.0;
            for (int q = 1; q < route_piece::k_pts; ++q)
            {
                const double ax = pc.x[q - 1], ay = pc.y[q - 1];
                const double vx = pc.x[q] - ax, vy = pc.y[q] - ay;
                const double l2 = vx * vx + vy * vy;
                const double u = l2 > 0.0 ? std::clamp(((qx - ax) * vx + (qy - ay) * vy) / l2, 0.0, 1.0) : 0.0;
                const double dx = qx - (ax + vx * u), dy = qy - (ay + vy * u);
                const double d2 = dx * dx + dy * dy;
                if (d2 < bd2)
                {
                    bd2 = d2;
                    bs  = pc.cum[q - 1] + u * (pc.cum[q] - pc.cum[q - 1]);
                }
            }
            const double d = std::sqrt(bd2);
            const double e = d - hw;
            if (e < 0.0)
                ++h.covers;
            if (e < h.e)
            {
                const double total = pc.cum[route_piece::k_pts - 1];
                h.e = e; h.d = d; h.hw = hw; h.tier = pc.tier; h.kind = pc.kind; h.tile = t;
                if (pc.kind == route_piece::curve)
                {
                    if (bs <= pc.split) { h.along = bs;         h.half = pc.split; }
                    else                { h.along = total - bs; h.half = total - pc.split; }
                }
                else
                {
                    h.along = total - bs; // pinned at the far end: the shared midpoint
                    h.half  = total;
                }
            }
        }
    }
}

inline void mixc(float& r, float& g, float& b, float tr, float tg, float tb, double a)
{
    const float af = static_cast<float>(std::clamp(a, 0.0, 1.0));
    r += (tr - r) * af;
    g += (tg - g) * af;
    b += (tb - b) * af;
}

/// The 0..1 coverage of a dash pattern centred on whole phases: @p along
/// arclength into a half of length @p half, dashes @p period apart, each
/// @p duty of the period long, anti-aliased over one pixel @p pxc.
inline double dashes(double along, double half, double period, double duty, double pxc)
{
    const double n = std::max(1.0, std::round(half / period));
    const double per = half / n;                 // a whole number of dashes per half
    const double ph = along / per;
    const double f = ph - std::floor(ph);
    const double dist = std::min(f, 1.0 - f) * per; // arclength to the dash centre
    return std::clamp((0.5 * duty * per - dist) / pxc + 0.5, 0.0, 1.0);
}

} // namespace

// =============================================================================
// The plan and the derivation
// =============================================================================

road_plan tile_road_plan(const bake_source& s, std::size_t i)
{
    road_plan pl;
    const std::size_t n = static_cast<std::size_t>(s.gw) * s.gh;
    if (i >= n || s.route_links.size() != n || s.inst.of_tile.size() != n)
        return pl;
    const std::int32_t li = s.inst.of_tile[i];
    if (li < 0)
        return pl;
    const tile_installation& ti = s.inst.list[static_cast<std::size_t>(li)];
    if (ti.n_stacks == 0 || ti.settlement.subject != stamp_subject::none)
        return pl;
    const std::uint8_t links = s.route_links[i] & 0x0Fu;
    if (!links)
        return pl;
    const int r = static_cast<int>(i) / s.gw;
    double ang[4];
    int deg = 0;
    for (int k = 0; k < 4; ++k)
        if (links & (1u << k))
        {
            double mx, my;
            link_mid(r, k, mx, my);
            ang[deg++] = std::atan2(my, mx);
        }
    // The free side: of 24 headings, the one farthest by angle from every
    // link; north first, then alternating either side of it, so a tie prefers
    // the cluster BEHIND the road (the road runs past its front).
    double best = -1.0, th = -kPi * 0.5;
    for (int k = 0; k < 24; ++k)
    {
        const int step = (k + 1) / 2;
        const double cand = -kPi * 0.5 + ((k & 1) ? 1.0 : -1.0) * step * (kPi / 12.0);
        double score = 1e9;
        for (int a = 0; a < deg; ++a)
        {
            double dlt = std::fabs(cand - ang[a]);
            dlt = std::fmod(dlt, 2.0 * kPi);
            if (dlt > kPi) dlt = 2.0 * kPi - dlt;
            score = std::min(score, dlt);
        }
        score = std::round(score * 1e6) / 1e6;
        if (score > best)
        {
            best = score;
            th = cand;
        }
    }
    pl.roaded = true;
    pl.scale  = deg == 1 ? 0.80 : deg == 2 ? 0.70 : 0.60;
    pl.radius = 0.72 * pl.scale;
    const double kappa = std::min(0.30, 0.84 - pl.radius);
    pl.fx = std::cos(th);
    pl.fy = std::sin(th);
    pl.kx = kappa * pl.fx;
    pl.ky = kappa * pl.fy;
    pl.apx = pl.kx - pl.fx * (pl.radius + 0.05);
    pl.apy = pl.ky - pl.fy * (pl.radius + 0.05);
    return pl;
}

void rederive_routes(bake_source& s)
{
    const std::size_t n = static_cast<std::size_t>(s.gw) * s.gh;
    s.road.resize(n, 0);
    s.lane.resize(n, 0);
    s.route_links.assign(n, 0);
    s.near_route.assign(n, 0);
    s.route_first.assign(n, 0);
    s.route_count.assign(n, 0);
    s.route_pieces.clear();
    if (n == 0)
        return;
    for (std::size_t i = 0; i < n; ++i)
    {
        const int r = static_cast<int>(i) / s.gw, c = static_cast<int>(i) % s.gw;
        std::uint8_t bits = 0;
        if (s.road[i])
            bits |= links_of(s.road, s, c, r, false);
        if (s.lane[i])
            bits |= static_cast<std::uint8_t>(links_of(s.lane, s, c, r, true) << 4);
        s.route_links[i] = bits;
    }
    for (std::size_t i = 0; i < n; ++i)
    {
        s.route_first[i] = static_cast<std::int32_t>(s.route_pieces.size());
        if (s.road[i])
        {
            const road_plan pl = tile_road_plan(s, i);
            tile_pieces(s, i, s.route_links[i] & 0x0Fu, s.road[i], &pl, s.route_pieces);
        }
        if (s.lane[i])
            tile_pieces(s, i, static_cast<std::uint8_t>(s.route_links[i] >> 4),
                        static_cast<std::uint8_t>(0x80u | s.lane[i]), nullptr, s.route_pieces);
        s.route_count[i] = static_cast<std::uint8_t>(
            std::min<std::size_t>(255, s.route_pieces.size() - static_cast<std::size_t>(s.route_first[i])));
    }
    // The cull: a point whose nearest tile is i can be reached only by the
    // pieces of i and its six neighbours.
    for (std::size_t i = 0; i < n; ++i)
    {
        std::uint8_t bits = 0;
        for (int k = -1; k < 6; ++k)
        {
            const int t = k < 0 ? static_cast<int>(i) : nb_index(s, static_cast<int>(i), k);
            if (t < 0)
                continue;
            if (s.road[static_cast<std::size_t>(t)]) bits |= 1u;
            if (s.lane[static_cast<std::size_t>(t)]) bits |= 2u;
        }
        s.near_route[i] = bits;
    }
}

void extract_routes(const world& w, entity_id body, bake_source& s)
{
    const std::size_t n = static_cast<std::size_t>(s.gw) * s.gh;
    s.road.assign(n, 0);
    s.lane.assign(n, 0);
    const auto land  = static_cast<std::uint8_t>(bake_source::tile_class::land);
    const auto water = static_cast<std::uint8_t>(bake_source::tile_class::water);
    for (const auto& [id, t] : w.tiles)
    {
        if (t.body != body || t.grid_x < 0 || t.grid_x >= s.gw || t.grid_y < 0 || t.grid_y >= s.gh)
            continue;
        const std::size_t i = static_cast<std::size_t>(t.grid_y) * s.gw + t.grid_x;
        // Survey mask: a masked tile carries no route (its class is neither).
        if (s.cls[i] == land && t.road_level > 0)
            s.road[i] = static_cast<std::uint8_t>(std::min<int>(t.road_level, k_route_highway));
        if (s.cls[i] == water && t.lane_level > 0)
            s.lane[i] = t.lane_level;
    }
    rederive_routes(s);
}

double route_clearance(const bake_source& s, double x, double y)
{
    if (!has_routes(s) || s.route_pieces.empty())
        return 1e9;
    route_hit h;
    query(s, x, y, /*lanes=*/false, h, nullptr, nullptr, /*slack=*/0.6);
    if (h.tile < 0)
        return 1e9;
    return h.e - tier_out(h.tier);
}

// =============================================================================
// The pass
// =============================================================================

void paint_routes(const bake_source& s, const geometry& g, const bake_params& p,
                  int px0, int py0, int pw, int ph, std::uint32_t* out,
                  const std::uint8_t* tag, const std::uint8_t* cover, const float* ground_lift)
{
    if (p.route_strength <= 0.0f || !has_routes(s) || s.route_pieces.empty())
        return;
    const double period = g.gw * kSqrt3;
    const auto periodic_cell = [&](double target, int& cells_out) -> double {
        cells_out = std::max(1, static_cast<int>(std::lround(period / target)));
        return period / cells_out;
    };
    int wear_n, speck_n, wake_n, foam_n;
    const double wear_c  = periodic_cell(0.32, wear_n);
    const double speck_c = periodic_cell(0.012, speck_n);
    const double wake_c  = periodic_cell(0.24, wake_n);
    const double foam_c  = periodic_cell(0.022, foam_n);
    const double ns   = nominal_s(g);
    const double pxc  = 1.0 / g.s;          // one actual pixel (edge coverage)
    const bool   fine = ns >= 40.0;         // sub-surface detail: ruts, lines, sleepers, speckle (nominal-keyed)
    const bool   mid  = ns >= 20.0;         // verges, ditches, kerbs
    const double k    = std::clamp(static_cast<double>(p.route_strength), 0.0, 1.0);
    const float  Lx = -0.554700196f, Ly = -0.832050323f; // the NW light every pass uses
    const auto   land = static_cast<std::uint8_t>(bake_source::tile_class::land);

    for (int y = 0; y < ph; ++y)
        for (int x = 0; x < pw; ++x)
        {
            const std::size_t idx = static_cast<std::size_t>(y) * pw + x;
            if (!tag[idx] || cover[idx] == 0)
                continue;
            const bool water = cover[idx] == 100;
            const double qx = (px0 + x + 0.5) / g.s;
            const double qy = (py0 + y + 0.5) / g.s + g.y_min + ground_lift[idx];
            route_hit h;
            int t0 = -1;
            double cx0 = 0.0;
            query(s, qx, qy, water, h, &t0, &cx0);
            if (h.tile < 0)
                continue;
            const double reach = h.e - tier_out(h.tier);
            if (reach >= 0.0)
                continue;
            // Absolute position for the noise, from the WRAPPED tile centre.
            const int c0w = t0 % s.gw;
            const double abx = qx - cx0 + kSqrt3 * (c0w + (((t0 / s.gw) & 1) ? 0.5 : 0.0));
            const double aby = qy;

            std::uint32_t& dst = out[idx];
            float r_ = static_cast<float>(palette::col_r(dst));
            float g_ = static_cast<float>(palette::col_g(dst));
            float b_ = static_cast<float>(palette::col_b(dst));
            const float gr = r_, gg = g_, gb = b_; // the ground under the road
            const double d = h.d, hw = h.hw;
            const double cov = std::clamp((hw - d) / pxc + 0.5, 0.0, 1.0); // the surface, anti-aliased
            const bool junction = h.covers >= 2;
            const bool lined = !junction && (h.kind == route_piece::curve || h.kind == route_piece::spoke);
            const bool edged = h.kind == route_piece::curve || h.kind == route_piece::spoke; // verges, ditches, shoulders

            if (water)
            {
                // THE WAKE: a faint, broken, pale trail on the lane's water.
                const double prof = 1.0 - smooth01(0.0, hw * 1.7, d);
                const double brk  = smooth01(0.38, 0.62, rnoise(abx, aby, wake_c, wake_n, 0x3A11u));
                double a = 0.15 * prof;
                if (mid)
                {
                    const double edge = std::clamp(1.0 - std::fabs(d - hw * 0.85) / 0.012, 0.0, 1.0);
                    a += 0.08 * edge;
                }
                if (fine && d < hw * 1.3) // soft flecks of foam, never a pixel grid
                    a += 0.18 * smooth01(0.66, 0.84, rnoise(abx, aby, foam_c, foam_n, 0x3A12u))
                              * (1.0 - smooth01(hw * 0.9, hw * 1.3, d));
                mixc(r_, g_, b_, 216.0f, 230.0f, 236.0f, a * brk * k);
                dst = palette::col32(std::clamp(static_cast<int>(r_ + 0.5f), 0, 255),
                                     std::clamp(static_cast<int>(g_ + 0.5f), 0, 255),
                                     std::clamp(static_cast<int>(b_ + 0.5f), 0, 255), 255);
                continue;
            }

            // The light: the ground's slope under the road, interpolated over
            // the nearest centres, from the NW light like the hillshade.
            double L = 1.0;
            {
                double gx = 0.0, gy = 0.0, ws = 0.0;
                int cc = 0, rr0 = 0;
                const int tn = nearest_tile(s, qx, qy, cc, rr0);
                if (tn >= 0)
                {
                    const double ccx = kSqrt3 * (cc + ((rr0 & 1) ? 0.5 : 0.0)), ccy = 1.5 * rr0;
                    for (int kk = -1; kk < 6; ++kk)
                    {
                        const int t = kk < 0 ? tn : nb_index(s, tn, kk);
                        if (t < 0 || s.cls[static_cast<std::size_t>(t)] != land)
                            continue;
                        const double dx = qx - ccx - (kk < 0 ? 0.0 : kNbDx[kk]);
                        const double dy = qy - ccy - (kk < 0 ? 0.0 : kNbDy[kk]);
                        const double w = std::max(0.0, 2.1 - (dx * dx + dy * dy));
                        gx += w * w * s.grad_x[static_cast<std::size_t>(t)];
                        gy += w * w * s.grad_y[static_cast<std::size_t>(t)];
                        ws += w * w;
                    }
                    if (ws > 0.0)
                        L = 1.0 + std::clamp((gx * Lx + gy * Ly) / ws * p.relief_gain * 0.8, -0.40, 0.40);
                }
            }
            const double wear  = 0.92 + 0.16 * rnoise(abx, aby, wear_c, wear_n, 0x3A01u);
            const double speck = fine ? (rspeck(abx, aby, speck_c, speck_n, 0x3A02u) - 0.5) : 0.0;
            const double lit = L * wear;

            switch (h.tier)
            {
                case k_route_track:
                {
                    // A worn margin, then dirt with wheel ruts and a grass crown.
                    const double outer = hw + tier_out(h.tier);
                    mixc(r_, g_, b_, 128.0f * static_cast<float>(lit), 108.0f * static_cast<float>(lit),
                         80.0f * static_cast<float>(lit), 0.30 * (1.0 - smooth01(hw, outer, d)) * k);
                    double sr = 140.0 * lit * (1.0 + 0.16 * speck), sg = 122.0 * lit * (1.0 + 0.16 * speck),
                           sb = 94.0 * lit * (1.0 + 0.16 * speck);
                    if (fine && lined)
                    {
                        const double u = d / hw;
                        const double rut = std::clamp(1.0 - std::fabs(u - 0.58) / 0.18, 0.0, 1.0);
                        sr *= 1.0 - 0.20 * rut; sg *= 1.0 - 0.21 * rut; sb *= 1.0 - 0.18 * rut;
                        const double crown = 1.0 - smooth01(0.06, 0.22, u);
                        sr += (gr * 0.98 - sr) * 0.50 * crown;
                        sg += (gg * 1.02 - sg) * 0.50 * crown;
                        sb += (gb * 0.96 - sb) * 0.50 * crown;
                    }
                    mixc(r_, g_, b_, static_cast<float>(sr), static_cast<float>(sg), static_cast<float>(sb), cov * k);
                    break;
                }
                case k_route_road:
                {
                    // Shallow ditches, grass verges, packed gravel.
                    if (mid && edged)
                    {
                        const double dc = hw + 0.031;
                        const double ditch = std::clamp(1.0 - std::fabs(d - dc) / 0.013, 0.0, 1.0);
                        const double dk = ditch * ditch * 0.32 * k;
                        r_ *= static_cast<float>(1.0 - dk);
                        g_ *= static_cast<float>(1.0 - dk * 0.80);
                        b_ *= static_cast<float>(1.0 - dk * 0.90);
                        const double verge = smooth01(hw - pxc, hw + pxc, d) * (1.0 - smooth01(hw + 0.014, hw + 0.020, d));
                        mixc(r_, g_, b_, gr * 1.06f + 8.0f, gg * 1.10f + 12.0f, gb * 0.98f,
                             0.45 * verge * k);
                    }
                    double base = 1.0 + 0.24 * speck;
                    if (fine && lined)
                    {
                        const double u = d / hw;
                        base *= 1.0 - 0.06 * std::clamp(1.0 - std::fabs(u - 0.45) / 0.16, 0.0, 1.0); // wheel tracks
                        base *= 1.0 + 0.04 * (1.0 - smooth01(0.0, 0.3, u));                         // the crown
                    }
                    const double sr = 166.0 * lit * base, sg = 154.0 * lit * base, sb = 128.0 * lit * base;
                    mixc(r_, g_, b_, static_cast<float>(sr), static_cast<float>(sg), static_cast<float>(sb), cov * k);
                    break;
                }
                case k_route_highway:
                {
                    // A gravel shoulder, asphalt, kerbs, a dashed centre line.
                    if (mid && edged)
                    {
                        const double sh = smooth01(hw - pxc, hw + pxc, d) * (1.0 - smooth01(hw + 0.016, hw + 0.026, d));
                        mixc(r_, g_, b_, 148.0f * static_cast<float>(L), 142.0f * static_cast<float>(L),
                             126.0f * static_cast<float>(L), 0.55 * sh * k);
                    }
                    const double base = (0.96 + 0.08 * wear) * (1.0 + 0.10 * speck);
                    double sr = 66.0 * L * base, sg = 68.0 * L * base, sb = 73.0 * L * base;
                    if (mid)
                    {
                        const double kerb = std::clamp((d - (hw - 0.011)) / pxc + 0.5, 0.0, 1.0);
                        sr += (184.0 * L - sr) * kerb; sg += (182.0 * L - sg) * kerb; sb += (174.0 * L - sb) * kerb;
                    }
                    if (fine && lined)
                    {
                        const double line = std::clamp((0.0045 - d) / pxc + 0.5, 0.0, 1.0)
                                          * dashes(h.along, h.half, 0.15, 0.55, pxc);
                        sr += (230.0 - sr) * line; sg += (214.0 - sg) * line; sb += (148.0 - sb) * line;
                    }
                    mixc(r_, g_, b_, static_cast<float>(sr), static_cast<float>(sg), static_cast<float>(sb), cov * k);
                    break;
                }
                default: // k_route_rail
                {
                    // Ballast spill, the bed, sleepers across it, twin rails.
                    if (mid)
                        mixc(r_, g_, b_, 120.0f * static_cast<float>(L), 114.0f * static_cast<float>(L),
                             104.0f * static_cast<float>(L),
                             0.40 * (1.0 - smooth01(hw, hw + tier_out(h.tier), d)) * k);
                    const double base = 1.0 + 0.36 * speck;
                    double sr = 130.0 * lit * base, sg = 122.0 * lit * base, sb = 112.0 * lit * base;
                    if (fine && !junction && h.kind != route_piece::yard)
                    {
                        const double across = std::clamp((0.78 * hw - d) / pxc + 0.5, 0.0, 1.0);
                        const double sl = across * dashes(h.along, h.half, 0.040, 0.36, pxc);
                        sr += (88.0 * L - sr) * sl; sg += (68.0 * L - sg) * sl; sb += (50.0 * L - sb) * sl;
                        const double rd = std::fabs(d - 0.40 * hw);
                        const double under = std::clamp((0.0080 - rd) / pxc + 0.5, 0.0, 1.0);
                        sr *= 1.0 - 0.30 * under; sg *= 1.0 - 0.30 * under; sb *= 1.0 - 0.30 * under;
                        const double rail = std::clamp((0.0045 - rd) / pxc + 0.5, 0.0, 1.0);
                        sr += (182.0 * L - sr) * rail; sg += (186.0 * L - sg) * rail; sb += (192.0 * L - sb) * rail;
                    }
                    mixc(r_, g_, b_, static_cast<float>(sr), static_cast<float>(sg), static_cast<float>(sb), cov * k);
                    break;
                }
            }
            dst = palette::col32(std::clamp(static_cast<int>(r_ + 0.5f), 0, 255),
                                 std::clamp(static_cast<int>(g_ + 0.5f), 0, 255),
                                 std::clamp(static_cast<int>(b_ + 0.5f), 0, 255), 255);
        }
}

// =============================================================================
// Hash and the partial re-bake
// =============================================================================

namespace {

/// Tiles whose route can change a pixel of the window, as unwrapped columns:
/// the window grown by @p margin canonical units (and the oblique lift above).
template <class F>
void walk_tiles(const bake_source& s, const geometry& g, int px0, int py0, int pw, int ph,
                double margin, F&& f)
{
    const double x0 = px0 / g.s - margin, x1 = (px0 + pw) / g.s + margin;
    const double y0 = py0 / g.s + g.y_min - margin;
    const double y1 = (py0 + ph) / g.s + g.y_min + margin + g.lift;
    const int r_lo = std::max(0, static_cast<int>(std::floor(y0 / 1.5)));
    const int r_hi = std::min(s.gh - 1, static_cast<int>(std::ceil(y1 / 1.5)));
    for (int r = r_lo; r <= r_hi; ++r)
    {
        const double odd = (r & 1) ? 0.5 : 0.0;
        const int c_lo = static_cast<int>(std::floor(x0 / kSqrt3 - odd)) - 1;
        const int c_hi = static_cast<int>(std::ceil(x1 / kSqrt3 - odd)) + 1;
        for (int c = c_lo; c <= c_hi; ++c)
            f(c, r, static_cast<std::size_t>(r) * s.gw + ((c % s.gw) + s.gw) % s.gw);
    }
}

/// The route-dependency reach: a pixel is painted by pieces of tiles within
/// ~1.1 of it, a tree beside a road stands up to ~1.9 above its root, a
/// piece is decided by tiles two rings out (a lane's rung test), and a
/// roaded cluster reaches as far as any structure.
constexpr double k_route_margin = 7.0;

bool same_pieces(const bake_source& a, const bake_source& b, std::size_t i)
{
    const bool ha = has_routes(a), hb = has_routes(b);
    const int na = ha ? a.route_count[i] : 0, nb = hb ? b.route_count[i] : 0;
    if (na != nb)
        return false;
    for (int j = 0; j < na; ++j)
        if (std::memcmp(&a.route_pieces[static_cast<std::size_t>(a.route_first[i] + j)],
                        &b.route_pieces[static_cast<std::size_t>(b.route_first[i] + j)],
                        sizeof(route_piece)) != 0)
            return false;
    return true;
}

bool same_plan(const road_plan& x, const road_plan& y)
{
    return x.roaded == y.roaded && x.kx == y.kx && x.ky == y.ky && x.scale == y.scale;
}

} // namespace

std::uint64_t route_hash(const bake_source& s, const geometry& g, int px0, int py0, int pw, int ph)
{
    if (!has_routes(s) || s.route_pieces.empty())
        return 0; // no route anywhere: nothing to fold
    std::uint64_t h = 0x52A7E5D1C0FFEE11ull;
    bool any = false;
    auto mix = [&h](std::uint64_t v) { h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2); };
    walk_tiles(s, g, px0, py0, pw, ph, k_route_margin, [&](int, int, std::size_t i) {
        if (s.road[i] | s.lane[i])
        {
            any = true;
            mix((static_cast<std::uint64_t>(i) << 16) | (static_cast<std::uint64_t>(s.road[i]) << 8) | s.lane[i]);
        }
    });
    // No route in reach: nothing to fold, so a window far from every road keeps
    // its hash when roads are laid elsewhere (no needless sweep job).
    return any ? h : 0;
}

int route_patch_boxes(const bake_source& a, const bake_source& b, const geometry& g,
                      const bake_params& p, int px0, int py0, int pw, int ph,
                      std::vector<pixel_rect>& out)
{
    if (a.gw != b.gw || a.gh != b.gh || b.gw <= 0)
        return -1;
    if (!has_routes(a) && !has_routes(b))
        return 0;
    const bool trees = nominal_s(g) >= 40.0 && p.tree_density > 0.0f;
    const auto land = static_cast<std::uint8_t>(bake_source::tile_class::land);
    int n = 0;
    const auto box = [&](double cx, double cy, double x0, double y0, double x1, double y1) {
        out.push_back({ static_cast<int>(std::floor((cx + x0) * g.s)) - 2,
                        static_cast<int>(std::floor((cy + y0 - g.y_min) * g.s)) - 2,
                        0, 0 });
        pixel_rect& r = out.back();
        const int ex = static_cast<int>(std::ceil((cx + x1) * g.s)) + 2;
        const int ey = static_cast<int>(std::ceil((cy + y1 - g.y_min) * g.s)) + 2;
        r.w = ex - r.x0;
        r.h = ey - r.y0;
        ++n;
    };
    // A tree whose root stands within its crown of a road's reach is left out
    // of the stamp, and one whose canopy or shadow stands over the road draws
    // after it (ground_bake.cpp stamp_trees): its root lies within ~0.55 north
    // or 0.15 south of the reach, its canopy reaches 0.24 sideways and down
    // from the root, and stands up to 0.55 plus the oblique lift above it.
    const double t_side = 0.50, t_down = 0.60 + 0.24 + 0.05, t_up = 0.15 + 0.55 + 0.30;
    walk_tiles(b, g, px0, py0, pw, ph, 4.0, [&](int c, int r, std::size_t i) {
        const road_plan pa = has_routes(a) ? tile_road_plan(a, i) : road_plan{};
        const road_plan pb = has_routes(b) ? tile_road_plan(b, i) : road_plan{};
        const bool pieces_moved = !same_pieces(a, b, i);
        const bool plan_moved = !same_plan(pa, pb);
        if (!pieces_moved && !plan_moved)
            return;
        const double cx = kSqrt3 * (c + ((r & 1) ? 0.5 : 0.0)), cy = 1.5 * r;
        // Trees stand only on forest and scrub; a tree a piece can touch roots
        // on the piece's own tile or a neighbour (within ~1.5 of its centre).
        const auto wooded_at = [&](int t) {
            if (t < 0 || b.cls[static_cast<std::size_t>(t)] != land)
                return false;
            const auto cv = static_cast<terrain_cover>(b.cover[static_cast<std::size_t>(t)]);
            return cv == terrain_cover::forest || cv == terrain_cover::scrub;
        };
        bool wooded = false;
        for (int kk = -1; kk < 6 && trees && !wooded; ++kk)
            wooded = wooded_at(kk < 0 ? static_cast<int>(i) : nb_index(b, static_cast<int>(i), kk));
        const auto settled_at = [&](const bake_source& src, int t) {
            if (src.inst.of_tile.size() != src.cls.size() || src.inst.of_tile[static_cast<std::size_t>(t)] < 0)
                return false;
            return src.inst.list[static_cast<std::size_t>(src.inst.of_tile[static_cast<std::size_t>(t)])]
                       .settlement.subject == stamp_subject::settlement;
        };
        if (pieces_moved)
        {
            // The oblique lift a pixel here can be drawn at: the ground's
            // height there is a blend of centres within two rings of the tile.
            double hmax = b.height[i];
            for (int k1 = 0; k1 < 6; ++k1)
            {
                const int t1 = nb_index(b, static_cast<int>(i), k1);
                if (t1 < 0)
                    continue;
                hmax = std::max(hmax, static_cast<double>(b.height[static_cast<std::size_t>(t1)]));
                for (int k2 = 0; k2 < 6; ++k2)
                {
                    const int t2 = nb_index(b, t1, k2);
                    if (t2 >= 0)
                        hmax = std::max(hmax, static_cast<double>(b.height[static_cast<std::size_t>(t2)]));
                }
            }
            const double hl = std::clamp(hmax, 0.0, 1.0) * g.lift;
            for (const bake_source* src : { &a, &b })
            {
                if (!has_routes(*src))
                    continue;
                const int cnt = src->route_count[i];
                for (int j = 0; j < cnt; ++j)
                {
                    const route_piece& pc = src->route_pieces[static_cast<std::size_t>(src->route_first[i] + j)];
                    const double reach = tier_hw(pc.tier) * 1.5 + tier_out(pc.tier);
                    // Only a road clears trees (a lane lies on open water),
                    // and only where trees grow.
                    const bool road = (pc.tier & 0x80u) == 0 && wooded;
                    // One box per half (a bend's two halves box far tighter
                    // than the whole curve), grown by the reach, the lift and
                    // the trees.
                    constexpr int mid = route_piece::k_pts / 2;
                    for (int half = 0; half < 2; ++half)
                    {
                        const int k0 = half ? mid : 0, k1 = half ? route_piece::k_pts - 1 : mid;
                        double x0 = pc.x[k0], x1 = pc.x[k0], y0 = pc.y[k0], y1 = pc.y[k0];
                        for (int k = k0 + 1; k <= k1; ++k)
                        {
                            x0 = std::min(x0, static_cast<double>(pc.x[k])); x1 = std::max(x1, static_cast<double>(pc.x[k]));
                            y0 = std::min(y0, static_cast<double>(pc.y[k])); y1 = std::max(y1, static_cast<double>(pc.y[k]));
                        }
                        box(cx, cy, x0 - reach - (road ? t_side : 0.0), y0 - reach - hl - (road ? t_up : 0.0),
                            x1 + reach + (road ? t_side : 0.0), y1 + reach + (road ? t_down : 0.0));
                    }
                }
            }
            // A town's blocks keep off the road: every town tile the pieces
            // pass near re-lays its blocks.
            for (int kk = -1; kk < 6; ++kk)
            {
                const int t = kk < 0 ? static_cast<int>(i) : nb_index(b, static_cast<int>(i), kk);
                if (t < 0 || !(settled_at(a, t) || settled_at(b, t)))
                    continue;
                // The neighbour's row and UNWRAPPED column: its centre beside ours.
                const int    tr  = r + (kk < 0 ? 0 : static_cast<int>(std::lround(kNbDy[kk] / 1.5)));
                const double ncx = cx + (kk < 0 ? 0.0 : kNbDx[kk]);
                const int ncol = static_cast<int>(std::lround(ncx / kSqrt3 - ((tr & 1) ? 0.5 : 0.0)));
                int x0, y0, x1, y1;
                for (const bake_source* src : { &a, &b })
                    if (installation_tile_bounds(*src, g, p, ncol, tr, x0, y0, x1, y1))
                    {
                        out.push_back({ x0, y0, x1 - x0, y1 - y0 });
                        ++n;
                    }
            }
        }
        if (plan_moved)
        {
            int x0, y0, x1, y1;
            for (const bake_source* src : { &a, &b })
                if (installation_tile_bounds(*src, g, p, c, r, x0, y0, x1, y1))
                {
                    out.push_back({ x0, y0, x1 - x0, y1 - y0 });
                    ++n;
                }
        }
        // The tile's own trees, when its cluster moved (a roaded cluster's
        // disc clears them).
        if (plan_moved && trees && wooded_at(static_cast<int>(i)))
        {
            const double side = 0.9 + 0.16 * 1.5;
            const double up   = 0.9 + g.lift + 2.85 * 0.16 / g.tilt_sy;
            const double down = 0.9 + 0.16 * 1.5;
            box(cx, cy, -side, -up, side, down);
        }
    });
    return n;
}

} // namespace ui::ground
