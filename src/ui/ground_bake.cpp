#include "ground_bake.hpp"

#include "terrain_palette.hpp"
#include "world/world.hpp"
#include "world/survey_system.hpp" // survey_tile_visible — the region mask (BL-067)

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ui::ground {

namespace {

constexpr double kSqrt3 = 1.7320508075688772;

/// Lock fill for survey-masked ground — PLANETARY.md's flat dark "locked" value.
constexpr std::uint32_t k_lock_colour = palette::col32(12, 14, 20, 255);

/// Deterministic 2-D lattice hash → [0, 1). FNV-1a over the lattice coords plus
/// a salt — the same spatial idiom the tile texture pass uses (grid-keyed,
/// never screen-keyed), so nothing in the bake can crawl or disagree between
/// wrap copies.
float hash01(int x, int y, std::uint32_t salt)
{
    std::uint32_t h = 2166136261u;
    auto mix = [&h](std::uint32_t v) { h ^= v; h *= 16777619u; h ^= h >> 13; };
    mix(static_cast<std::uint32_t>(x) * 73856093u);
    mix(static_cast<std::uint32_t>(y) * 19349663u);
    mix(salt * 83492791u);
    h ^= h >> 16;
    return static_cast<float>(h & 0x00FFFFFFu) / 16777216.0f;
}

/// One octave of value noise on a lattice of @p cell canonical units, bilinear,
/// wrap-periodic in x with period @p period_cells lattice cells. Smoothstep on
/// the fractions keeps it C1 — a hard lattice reads as a grid, which is the one
/// thing the ground must never do again.
float value_noise(double x, double y, double cell, int period_cells, std::uint32_t salt)
{
    const double fx = x / cell, fy = y / cell;
    int ix = static_cast<int>(std::floor(fx));
    int iy = static_cast<int>(std::floor(fy));
    float tx = static_cast<float>(fx - ix);
    float ty = static_cast<float>(fy - iy);
    tx = tx * tx * (3.0f - 2.0f * tx);
    ty = ty * ty * (3.0f - 2.0f * ty);
    auto wrap = [&](int v) { return ((v % period_cells) + period_cells) % period_cells; };
    const float v00 = hash01(wrap(ix),     iy,     salt);
    const float v10 = hash01(wrap(ix + 1), iy,     salt);
    const float v01 = hash01(wrap(ix),     iy + 1, salt);
    const float v11 = hash01(wrap(ix + 1), iy + 1, salt);
    const float a = v00 + (v10 - v00) * tx;
    const float b = v01 + (v11 - v01) * tx;
    return a + (b - a) * ty; // [0, 1)
}

inline float height_at(const bake_source& s, int c, int r)
{
    const int cw = ((c % s.gw) + s.gw) % s.gw;
    const int rc = std::clamp(r, 0, s.gh - 1);
    const std::size_t i = static_cast<std::size_t>(rc) * s.gw + cw;
    // Water flattens to its own height so a shoreline's slope lives on the land
    // side; a void tile contributes the clamped neighbour instead.
    return s.height[i];
}

} // namespace

geometry make_geometry(int gw, int gh, double target_px_per_r, double tilt_sy)
{
    geometry g;
    g.gw = gw;
    g.gh = gh;
    const double period = gw * kSqrt3; // canonical wrap period
    g.W = std::max(1, static_cast<int>(std::lround(period * target_px_per_r)));
    g.s = g.W / period; // re-derived so wrap copies abut exactly
    g.tilt_sy = std::clamp(tilt_sy, 0.3, 1.0);
    if (g.tilt_sy < 1.0)
    {
        const double tilt = std::acos(g.tilt_sy);
        g.lift = 0.9 * std::tan(tilt);
    }
    // Vertical extent: hex tops of row 0 reach y = -1, hex bottoms of the last
    // row reach 1.5 * (gh - 1) + 1. A tilted bake additionally holds displaced
    // peaks and standing trees above row 0 (lift + tree headroom).
    g.y_min = -1.0 - (g.tilt_sy < 1.0 ? g.lift + 0.5 / g.tilt_sy : 0.0);
    const double y_max = 1.5 * (gh - 1) + 1.0;
    g.H = std::max(1, static_cast<int>(std::ceil((y_max - g.y_min) * g.s)));
    return g;
}

bake_source prepare_source(const world& w, entity_id body, bool reveal_all)
{
    bake_source s;
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end())
        return s;
    const body_component& b = bit->second;
    s.gw = b.grid_width;
    s.gh = b.grid_height;
    const std::size_t n = static_cast<std::size_t>(s.gw) * s.gh;
    s.cls.assign(n, static_cast<std::uint8_t>(bake_source::tile_class::void_));
    s.colour.assign(n, 0u);
    s.height.assign(n, 0.0f);
    s.grad_x.assign(n, 0.0f);
    s.grad_y.assign(n, 0.0f);
    s.relief_bias.assign(n, 0.0f);
    s.jitter.assign(n, 0.0f);
    s.cover.assign(n, static_cast<std::uint8_t>(terrain_cover::none));
    s.density.assign(n, 0);

    for (const auto& [id, t] : w.tiles)
    {
        if (t.body != body)
            continue;
        if (t.grid_x < 0 || t.grid_x >= s.gw || t.grid_y < 0 || t.grid_y >= s.gh)
            continue;
        const std::size_t i = static_cast<std::size_t>(t.grid_y) * s.gw + t.grid_x;

        const bool water = t.substrate == terrain_substrate::ocean
                        || t.substrate == terrain_substrate::coast
                        || t.substrate == terrain_substrate::lake;
        const bool seen  = reveal_all
                        || survey_tile_visible(b.survey, s.gw, s.gh, t.grid_x, t.grid_y);

        if (!seen)
        {
            // Masked ground bakes as the flat lock colour and joins no blend:
            // the lock fill is a statement about knowledge, not terrain, and an
            // interpolated lock edge would leak the shape of unsurveyed ground.
            s.cls[i]    = static_cast<std::uint8_t>(bake_source::tile_class::masked);
            s.colour[i] = k_lock_colour;
            continue;
        }

        s.cls[i]    = static_cast<std::uint8_t>(water ? bake_source::tile_class::water
                                                      : bake_source::tile_class::land);
        s.colour[i] = palette::tile_colour(t.substrate, t.cover, t.cover_density);
        s.height[i] = t.height;
        s.relief_bias[i] = palette::relief_amount(t.landform);
        s.jitter[i] = hash01(t.grid_x, t.grid_y, 0xB732u) * 2.0f - 1.0f;
        s.cover[i]   = static_cast<std::uint8_t>(t.cover);
        s.density[i] = t.cover_density;
    }

    // Height gradient from neighbour differences — symmetric central
    // differences over the raster (columns wrap, rows clamp). Computed on the
    // tile grain and interpolated per pixel, which is both cheaper and smoother
    // than per-pixel finite differences of the interpolated field.
    for (int r = 0; r < s.gh; ++r)
        for (int c = 0; c < s.gw; ++c)
        {
            const std::size_t i = static_cast<std::size_t>(r) * s.gw + c;
            // col_step is sqrt(3) canonical units; row_step 1.5.
            s.grad_x[i] = static_cast<float>(
                (height_at(s, c + 1, r) - height_at(s, c - 1, r)) / (2.0 * kSqrt3));
            s.grad_y[i] = static_cast<float>(
                (height_at(s, c, r + 1) - height_at(s, c, r - 1)) / (2.0 * 1.5));
        }
    return s;
}

namespace {

/// Stamp individual tree canopies over the baked ground (the close tiers'
/// "individual trees"). Forest and scrub tiles scatter hash-positioned
/// canopies — count from cover density, positions/sizes/tints per-tile-hashed
/// so every wrap copy and every chunk agrees; the window is walked with a
/// margin so a canopy spanning a chunk edge renders identically in both
/// chunks. Each tree is a soft drop shadow toward the SE plus a canopy blob
/// lit from the NW — the same light every other pass uses. Stamps respect the
/// tag buffer: a lock-fill or transparent pixel is never painted, so nothing
/// leaks through the survey mask.
void stamp_trees(const bake_source& src, const geometry& g, const bake_params& p,
                 int px0, int py0, int pw, int ph, std::uint32_t* out,
                 const std::uint8_t* tag)
{
    // An (optionally elliptical) soft blob: bry > brx bakes the pre-stretched
    // verticals an oblique geometry needs, so a canopy squashes back to round
    // under the camera. The distance metric scales dy into the x radius, so
    // the AA edge stays ~1 px on every axis.
    const auto blob = [&](float bx, float by, float brx, float bry,
                          float cr2, float cg2, float cb2, float alpha, bool lit)
    {
        const int x0i = std::max(0, static_cast<int>(std::floor(bx - brx - 1.0f)));
        const int x1i = std::min(pw - 1, static_cast<int>(std::ceil(bx + brx + 1.0f)));
        const int y0i = std::max(0, static_cast<int>(std::floor(by - bry - 1.0f)));
        const int y1i = std::min(ph - 1, static_cast<int>(std::ceil(by + bry + 1.0f)));
        if (bry <= 0.0f || brx <= 0.0f)
            return;
        const float yscale = brx / bry;
        for (int py_ = y0i; py_ <= y1i; ++py_)
            for (int px_ = x0i; px_ <= x1i; ++px_)
            {
                const std::size_t idx = static_cast<std::size_t>(py_) * pw + px_;
                if (!tag[idx])
                    continue; // lock fill / transparent margin stays untouched
                const float dx = px_ + 0.5f - bx;
                const float dy = (py_ + 0.5f - by) * yscale;
                const float d  = std::sqrt(dx * dx + dy * dy);
                if (d >= brx + 0.8f)
                    continue;
                const float a = alpha * std::clamp((brx + 0.8f - d) / 1.6f, 0.0f, 1.0f);
                float rr = cr2, gg = cg2, bb = cb2;
                if (lit)
                {
                    // Highlight offset toward the NW light, shadowed SE rim.
                    const float lx = dx + brx * 0.35f, ly = dy + brx * 0.35f;
                    const float lt = std::clamp(
                        1.28f - 0.75f * std::sqrt(lx * lx + ly * ly) / brx, 0.55f, 1.28f);
                    rr *= lt; gg *= lt; bb *= lt;
                }
                std::uint32_t& dst = out[idx];
                const float ir = static_cast<float>(palette::col_r(dst));
                const float ig = static_cast<float>(palette::col_g(dst));
                const float ib = static_cast<float>(palette::col_b(dst));
                dst = palette::col32(
                    std::clamp(static_cast<int>(ir + (rr - ir) * a + 0.5f), 0, 255),
                    std::clamp(static_cast<int>(ig + (gg - ig) * a + 0.5f), 0, 255),
                    std::clamp(static_cast<int>(ib + (bb - ib) * a + 0.5f), 0, 255), 255);
            }
    };

    // Max canopy + shadow reach, canonical units; a tilted bake's standing
    // trees additionally rise by the height displacement plus their own
    // stretched height, so the walk margin grows with them.
    const double margin = 0.6 + (g.lift > 0.0 ? g.lift + 1.4 / g.tilt_sy : 0.0);
    const double wx0 = px0 / g.s - margin, wx1 = (px0 + pw) / g.s + margin;
    const double wy0 = py0 / g.s + g.y_min - margin;
    const double wy1 = (py0 + ph) / g.s + g.y_min + margin;
    const int r_lo = std::max(0, static_cast<int>(std::floor(wy0 / 1.5)));
    const int r_hi = std::min(src.gh - 1, static_cast<int>(std::ceil(wy1 / 1.5)));
    for (int r = r_lo; r <= r_hi; ++r)
    {
        const double odd = (r & 1) ? 0.5 : 0.0;
        const int c_lo = static_cast<int>(std::floor(wx0 / kSqrt3 - odd)) - 1;
        const int c_hi = static_cast<int>(std::ceil (wx1 / kSqrt3 - odd)) + 1;
        for (int c = c_lo; c <= c_hi; ++c)
        {
            const int cw = ((c % src.gw) + src.gw) % src.gw;
            const std::size_t i = static_cast<std::size_t>(r) * src.gw + cw;
            if (src.cls[i] != static_cast<std::uint8_t>(bake_source::tile_class::land))
                continue;
            const auto cov = static_cast<terrain_cover>(src.cover[i]);
            const bool forest = cov == terrain_cover::forest;
            const bool scrub  = cov == terrain_cover::scrub;
            if (!forest && !scrub)
                continue;
            const float dens = src.density[i] / 255.0f;
            const int   n = static_cast<int>(std::lround(
                (forest ? 6.0f + 13.0f * dens : 2.0f + 4.0f * dens) * p.tree_density));
            // Hashes key on the WRAPPED coordinate, positions on the unwrapped
            // centre: every wrap copy grows the same trees in the same places.
            const double hx = kSqrt3 * (c + odd);
            const double hy = 1.5 * r;
            const std::uint32_t tc = src.colour[i];
            for (int k = 0; k < n; ++k)
            {
                const float a1 = hash01(cw, r, 0x7E00u + static_cast<std::uint32_t>(k) * 3u);
                const float a2 = hash01(cw, r, 0x7E01u + static_cast<std::uint32_t>(k) * 3u);
                const float a3 = hash01(cw, r, 0x7E02u + static_cast<std::uint32_t>(k) * 3u);
                const double ang = a1 * 6.283185307;
                const double rad = 0.82 * std::sqrt(a2);
                const double tx  = hx + rad * std::cos(ang);
                const double ty  = hy + rad * std::sin(ang) * 0.9;
                const float  cr  = (0.085f + 0.055f * a3) * (forest ? 1.0f : 0.62f);
                const float  pxc = static_cast<float>(tx * g.s - px0);
                const float  pyc = static_cast<float>((ty - g.y_min) * g.s - py0);
                const float  pr  = cr * static_cast<float>(g.s);
                // Canopy ink: the tile's own colour pushed toward deep leaf,
                // varied per tree so a wood is a crowd, not a pattern.
                const float vr = 0.86f + 0.28f * hash01(cw, r, 0x7F00u + static_cast<std::uint32_t>(k));
                const float cr_ = (palette::col_r(tc) * 0.45f + 20.0f * 0.55f) * vr;
                const float cg_ = (palette::col_g(tc) * 0.45f + 62.0f * 0.55f) * vr;
                const float cb_ = (palette::col_b(tc) * 0.45f + 26.0f * 0.55f) * vr;
                if (g.lift > 0.0)
                {
                    // OBLIQUE: the tree STANDS. Its ground point rides the
                    // height displacement; the trunk and canopy bake with
                    // verticals stretched by 1/tilt_sy so the camera squash
                    // returns them to true proportion. Shadow stays on the
                    // ground plane — the depth cue that sells the tilt.
                    const float invsy = static_cast<float>(1.0 / g.tilt_sy);
                    const float ygr   = static_cast<float>(
                        (ty - src.height[i] * g.lift - g.y_min) * g.s - py0);
                    const float th = pr * 1.1f * invsy; // trunk height, px
                    blob(pxc + pr * 0.35f, ygr + pr * 0.22f, pr * 0.95f, pr * 0.62f,
                         10.0f, 14.0f, 10.0f, 0.30f, false);         // ground shadow
                    blob(pxc, ygr - th * 0.5f, std::max(1.2f, pr * 0.16f), th * 0.5f,
                         46.0f, 36.0f, 26.0f, 0.90f, false);         // trunk
                    blob(pxc, ygr - th - pr * invsy * 0.75f, pr, pr * invsy,
                         cr_, cg_, cb_, 0.95f, true);                // canopy, upright
                }
                else
                {
                    blob(pxc + pr * 0.45f, pyc + pr * 0.42f, pr * 1.0f, pr * 1.0f,
                         10.0f, 14.0f, 10.0f, 0.30f, false);         // drop shadow, SE
                    blob(pxc, pyc, pr, pr, cr_, cg_, cb_, 0.94f, true); // canopy, lit NW
                }
            }
        }
    }
}

} // namespace

namespace {

/// The per-pixel base bake for one window: interpolated colour, hillshade,
/// grain, mottle — UNGRADED, stamps and post passes are the orchestrator's
/// (bake_region below). Fills @p tag (1 = a terrain pixel later passes may
/// touch; 0 = transparent margin or the survey lock fill, which must stay
/// EXACT) and @p cover_out (0 = untouchable; 100 = water; 1+cover = land
/// cover class — the edge-ink pass reads it, and because it is resolved at
/// the WARPED sample point, an inked boundary follows the organic edge for
/// free).
void bake_window(const bake_source& src, const geometry& g, const bake_params& p,
                 int px0, int py0, int pw, int ph, std::uint32_t* out,
                 std::uint8_t* tag, std::uint8_t* cover_out)
{
    const double period   = g.gw * kSqrt3;
    // Resolution-adaptive character (wave 2). The interpolation radius and the
    // detail amplitudes are CANONICAL-scale, so the same numbers that read as
    // painterly at the 24 px tier read as plain blur at 48/96 — a colour field
    // 1.4 tiles soft is 70 px soft up close. As the bake resolution grows past
    // the play tier, tighten the field and lift the detail: res_t is 0 at
    // 24 px/r and 1 at 96.
    const float  res_t = static_cast<float>(std::clamp((g.s - 24.0) / 48.0, 0.0, 1.0));
    const double R     = p.blend_radius * (1.0 - 0.22 * res_t); // stays > 1 (corner coverage)
    const double R2    = R * R;
    const float  detail_mul = 1.0f + 1.1f * res_t;
    const float  noise_mul  = 1.0f + 1.2f * res_t;
    // Weight EXPONENT sharpening: the coverage radius cannot drop below one
    // tile, so patch crispness at the close tiers comes from steepening the
    // falloff instead — a higher power hands the pixel to its nearest centre
    // and the colour field stops reading as mist without losing coverage.
    const int wpow = 2 + static_cast<int>(std::lround(5.0f * res_t));
    // Light from the north-west, the hillshade convention every panel-C read
    // leans on (and the same warm-up/cool-down direction the relief tint set).
    const float Lx = -0.554700196f, Ly = -0.832050323f;

    // Noise lattices must divide the wrap period EXACTLY or the grain carries a
    // seam at the cylinder join: pick the cell count nearest the target size,
    // then re-derive the cell from the period.
    const auto periodic_cell = [&](double target, int& cells_out) -> double
    {
        cells_out = std::max(1, static_cast<int>(std::lround(period / target)));
        return period / cells_out;
    };
    int noise_cells_a, noise_cells_b, warp_cells, warp_cells2, detail_cells, detail_cells2;
    const double noise_cell_a = periodic_cell(0.90, noise_cells_a);
    const double noise_cell_b = periodic_cell(0.37, noise_cells_b);
    const double warp_cell    = periodic_cell(p.warp_cell, warp_cells);
    const double warp_cell2   = periodic_cell(p.warp_cell * 0.29, warp_cells2);
    const double detail_cell  = periodic_cell(p.detail_cell, detail_cells);
    const double detail_cell2 = periodic_cell(p.detail_cell * 0.41, detail_cells2);
    // Close-tier grain: at high bake resolutions (the 48/96 px zoom tiers) the
    // standard octaves span many texels and the ground reads under-detailed up
    // close — one finer octave keys in on resolution alone.
    int fine_cells = 1;
    const double fine_cell = periodic_cell(0.155, fine_cells);
    const bool   fine_on   = g.s >= 40.0;

    for (int py = 0; py < ph; ++py)
    {
        const double y0_ = (py0 + py + 0.5) / g.s + g.y_min;
        std::uint32_t* row_out = out + static_cast<std::size_t>(py) * pw;

        for (int px = 0; px < pw; ++px)
        {
            const double x0_ = (px0 + px + 0.5) / g.s;

            // Domain warp: displace the sample point by a two-octave vector
            // field BEFORE resolving the owning tile. A class boundary then
            // follows the warped field instead of the hex lattice — the big
            // octave meanders the coastline, the small one FRAYS the straight
            // hex edges the big one merely translates. Periodic in x like
            // every other lattice here. The warp applies to the TILE lookup
            // only; the detail and grain fields below sample unwarped
            // coordinates, so a strong warp cannot swirl the brushwork.
            const double uy = y0_ - g.y_min;
            const double wax =
                (value_noise(x0_, uy, warp_cell,  warp_cells,  0xA11Cu) - 0.5) * 2.0
              + (value_noise(x0_, uy, warp_cell2, warp_cells2, 0xA21Cu) - 0.5) * 0.9;
            const double way =
                (value_noise(x0_, uy, warp_cell,  warp_cells,  0xB22Du) - 0.5) * 2.0
              + (value_noise(x0_, uy, warp_cell2, warp_cells2, 0xB32Du) - 0.5) * 0.9;
            const double x = x0_ + wax * p.warp_amp;
            const double y = y0_ + way * p.warp_amp;

            // Gather tile-centre candidates within the blend radius of a
            // sample point. Weights are a Wendland-style (R² − d²)^wpow
            // falloff — smooth, compact, cheap; the OWNER (nearest centre)
            // decides the pixel's class, and only candidates of that class
            // blend, so a coastline and a mask edge stay hard while
            // everything inside a class is continuous. A lambda because an
            // oblique bake resolves TWICE: once flat to learn the height,
            // then again at the height-displaced point.
            double best_d2 = 1e30;
            int    owner   = -1;
            std::uint8_t owner_cls = 0;
            struct cand { std::size_t i; double w; };
            cand cands[24];
            int  ncand = 0;
            const auto gather = [&](double sx, double sy_)
            {
                best_d2 = 1e30;
                owner   = -1;
                ncand   = 0;
                const int r_lo = std::max(0, static_cast<int>(std::ceil((sy_ - R) / 1.5)));
                const int r_hi = std::min(src.gh - 1,
                                          static_cast<int>(std::floor((sy_ + R) / 1.5)));
                for (int r = r_lo; r <= r_hi; ++r)
                {
                    const double cy   = 1.5 * r;
                    const double dy   = sy_ - cy;
                    const double odd  = (r & 1) ? 0.5 : 0.0;
                    const int    c0   = static_cast<int>(std::floor(sx / kSqrt3 - odd));
                    for (int dc = -1; dc <= 2; ++dc)
                    {
                        const int c  = c0 + dc;
                        const double cx = kSqrt3 * (c + odd);
                        double dx = sx - cx;
                        dx -= period * std::round(dx / period); // cylinder wrap
                        const double d2 = dx * dx + dy * dy;
                        if (d2 >= R2)
                            continue;
                        const int cw = ((c % src.gw) + src.gw) % src.gw;
                        const std::size_t i = static_cast<std::size_t>(r) * src.gw + cw;
                        if (src.cls[i] == static_cast<std::uint8_t>(bake_source::tile_class::void_))
                            continue;
                        const double t = R2 - d2;
                        double wgt = t * t;
                        for (int e = 2; e < wpow; ++e)
                            wgt *= t;
                        if (ncand < 24)
                            cands[ncand++] = { i, wgt };
                        if (d2 < best_d2)
                        {
                            best_d2 = d2;
                            owner   = static_cast<int>(i);
                        }
                    }
                }
            };

            gather(x, y);
            if (owner < 0)
            {
                row_out[px] = 0u; // outside the grid: transparent, canvas bg shows
                continue;
            }
            owner_cls = src.cls[static_cast<std::size_t>(owner)];
            if (owner_cls == static_cast<std::uint8_t>(bake_source::tile_class::masked))
            {
                row_out[px] = k_lock_colour; // flat, unblended, no leak
                continue;
            }

            // OBLIQUE (BL-737): displace by the smoothed height at this point
            // and re-resolve — the ground the camera sees at this row is the
            // terrain standing lift·h higher, so hills grow real silhouettes.
            // A masked re-resolve locks (a peak truncates at the mask edge
            // rather than leaking); the lift is modest, so the projection
            // stays monotonic and needs no occlusion handling.
            if (g.lift > 0.0)
            {
                double hw = 0.0, ww = 0.0;
                for (int k = 0; k < ncand; ++k)
                    if (src.cls[cands[k].i] == owner_cls)
                    {
                        hw += cands[k].w * src.height[cands[k].i];
                        ww += cands[k].w;
                    }
                const double h_est = ww > 0.0
                    ? hw / ww
                    : src.height[static_cast<std::size_t>(owner)];
                gather(x, y + h_est * g.lift);
                if (owner < 0)
                {
                    row_out[px] = 0u;
                    continue;
                }
                owner_cls = src.cls[static_cast<std::size_t>(owner)];
                if (owner_cls == static_cast<std::uint8_t>(bake_source::tile_class::masked))
                {
                    row_out[px] = k_lock_colour;
                    continue;
                }
            }

            cover_out[static_cast<std::size_t>(py) * pw + px] =
                owner_cls == static_cast<std::uint8_t>(bake_source::tile_class::water)
                    ? 100
                    : static_cast<std::uint8_t>(1 + src.cover[static_cast<std::size_t>(owner)]);
            double wsum = 0.0, cr = 0.0, cg = 0.0, cb = 0.0;
            double hsum = 0.0, gx = 0.0, gy = 0.0, rb = 0.0, jt = 0.0;

            for (int k = 0; k < ncand; ++k)
            {
                const std::size_t i = cands[k].i;
                if (src.cls[i] != owner_cls)
                    continue;
                const double wgt = cands[k].w;
                const std::uint32_t col = src.colour[i];
                wsum += wgt;
                cr += wgt * palette::col_r(col);
                cg += wgt * palette::col_g(col);
                cb += wgt * palette::col_b(col);
                hsum += wgt * src.height[i];
                gx   += wgt * src.grad_x[i];
                gy   += wgt * src.grad_y[i];
                rb   += wgt * src.relief_bias[i];
                jt   += wgt * src.jitter[i];
            }
            if (wsum <= 0.0)
            {
                const std::uint32_t col = src.colour[static_cast<std::size_t>(owner)];
                row_out[px] = col;
                tag[static_cast<std::size_t>(py) * pw + px] = 1;
                continue;
            }
            const double inv = 1.0 / wsum;
            float r_ = static_cast<float>(cr * inv);
            float g_ = static_cast<float>(cg * inv);
            float b_ = static_cast<float>(cb * inv);
            const float h    = static_cast<float>(hsum * inv);
            const float gxx  = static_cast<float>(gx * inv);
            const float gyy  = static_cast<float>(gy * inv);
            const float bias = static_cast<float>(rb * inv);
            const float jtt  = static_cast<float>(jt * inv);

            const bool land = owner_cls == static_cast<std::uint8_t>(bake_source::tile_class::land);

            float lum = 1.0f;
            if (land)
            {
                // Fractal height detail: a noise field ADDED to the interpolated
                // tile height before shading, its amplitude growing with the
                // landform bias — plains stay calm, a range reads craggy. The
                // hillshade then runs on the combined gradient (tile gradient +
                // finite-difference gradient of the detail field), which is
                // what gives the ground painterly terrain texture at sub-tile
                // scale instead of a per-hex mosaic.
                const float amp = p.detail_amp * detail_mul
                                * (0.25f + p.landform_accent * std::fabs(bias) + 0.35f * h);
                // The GRADIENT reads the low octave only, central-differenced
                // at half a cell: a fine octave in the slope is per-pixel
                // speckle, not terrain. The fine octave still contributes to
                // the VALUE below, where it reads as surface variation.
                //
                // RIDGED MIX (the "sharper hills" ruling, Ben 2026-09-01): on
                // strongly-biased landforms the smooth field folds toward
                // ridged noise (0.25 − |n − 0.5|), whose |·| kink puts a hard
                // crease at every crest — a range then shades as ridge lines
                // instead of soft blobs. The finite differences pick the
                // crease up for free.
                const float ridge_w = std::min(1.0f, std::fabs(bias) * p.landform_accent)
                                    * p.ridge_strength;
                const auto detail_lo = [&](double sx, double sy) -> float
                {
                    const float n = value_noise(sx, sy, detail_cell, detail_cells, 0xD371u);
                    const float smooth = n - 0.5f;
                    const float ridged = (0.25f - std::fabs(n - 0.5f)) * 2.0f;
                    return smooth + (ridged - smooth) * ridge_w;
                };
                const double eps = detail_cell * 0.5;
                const float ddx = (detail_lo(x0_ + eps, uy) - detail_lo(x0_ - eps, uy))
                                  / static_cast<float>(2.0 * eps);
                const float ddy = (detail_lo(x0_, uy + eps) - detail_lo(x0_, uy - eps))
                                  / static_cast<float>(2.0 * eps);
                const float d0 = detail_lo(x0_, uy) * 0.7f
                    + (value_noise(x0_, uy, detail_cell2, detail_cells2, 0xD372u) - 0.5f) * 0.3f;

                // Two shade terms with their own scales: tile slopes are tiny
                // (heights are 0-1 across a whole continent) and take the big
                // gain; the detail slope is order-1 and takes amp alone. The
                // clamp widens with resolution — a close tier is allowed
                // deeper shadow.
                const float tgx = gxx + ddx * amp;
                const float tgy = gyy + ddy * amp;
                const float shade = (gxx * Lx + gyy * Ly) * p.relief_gain
                                  + (ddx * Lx + ddy * Ly) * amp * 0.75f;
                lum += std::clamp(shade, -(0.60f + 0.15f * res_t), 0.60f + 0.15f * res_t);

                // Slope rock exposure: steep ground sheds its cover colour
                // toward bare rock, which is what makes a hillside read as a
                // HILL rather than as shaded grass. Strongest at close tiers.
                const float slope = std::sqrt(tgx * tgx + tgy * tgy);
                const float rock_t = std::clamp((slope - 0.55f) * 1.2f, 0.0f, 1.0f)
                                   * p.rock_exposure * (0.35f + 0.65f * res_t);
                if (rock_t > 0.0f)
                {
                    r_ += (122.0f - r_) * rock_t;
                    g_ += (112.0f - g_) * rock_t;
                    b_ += (100.0f - b_) * rock_t;
                }
                // Altitude lift, the landform's own signed bias, and the detail
                // field's own value (a crag's top is lit even side-on).
                lum += (h - 0.45f) * p.altitude_gain + bias * 0.30f + d0 * amp * 0.8f;
                // Painterly patchiness: a WHISPER of per-tile jitter (this is
                // the hex-mosaic dial — the detail field carries the texture
                // now), then fine grain.
                lum += jtt * p.jitter;
                const float n2 = value_noise(x0_, uy, noise_cell_b, noise_cells_b, 0xFAB1u);
                lum += (n2 - 0.5f) * 2.0f * p.noise_strength * noise_mul;
                if (fine_on)
                {
                    const float n3 = value_noise(x0_, uy, fine_cell, fine_cells, 0x51D3u);
                    lum += (n3 - 0.5f) * (1.2f + 2.2f * res_t) * p.noise_strength;
                }
            }
            else
            {
                // Water: keep it calm — depth already varies by kind (BL-516's
                // family), so only a whisper of grain so it does not read flat-shaded.
                const float n1 = value_noise(x0_, uy, noise_cell_a, noise_cells_a, 0x5EA0u);
                lum += (n1 - 0.5f) * 2.0f * p.water_noise;
                lum += (h - 0.45f) * 0.06f;
            }
            r_ *= lum; g_ *= lum; b_ *= lum;

            if (land)
            {
                // Colour mottle: a mid-frequency warm/cool swing so ground
                // varies in HUE as well as luminance — luminance noise alone
                // reads as mist over the smooth colour field, most of all at
                // the close tiers (hence the res_t growth).
                const float mot = (value_noise(x0_, uy, noise_cell_a, noise_cells_a, 0xC01Au)
                                   - 0.5f) * (0.6f + 0.9f * res_t);
                r_ *= 1.0f + mot * 0.14f;
                g_ *= 1.0f + mot * 0.04f;
                b_ *= 1.0f - mot * 0.10f;
            }

            // UNGRADED write: the near-future grade moved to a final buffer
            // sweep (below) so the feature stamps drawn between base and
            // grade take the grade exactly as the ground under them does —
            // the grade stays a separable pass, per the settled ruling.
            row_out[px] = palette::col32(
                std::clamp(static_cast<int>(r_ + 0.5f), 0, 255),
                std::clamp(static_cast<int>(g_ + 0.5f), 0, 255),
                std::clamp(static_cast<int>(b_ + 0.5f), 0, 255), 255);
            tag[static_cast<std::size_t>(py) * pw + px] = 1;
        }
    }
}

/// Darken the pixel where its right/down neighbour resolves a different cover
/// class — a deterministic 1 px ink line on every cover boundary and shoreline.
/// Runs on the apron buffer, so a line never breaks at a chunk edge; class 0
/// (lock fill / transparent) never inks and is never inked against.
void edge_ink(std::uint32_t* buf, const std::uint8_t* cover, int pw, int ph,
              float cover_k, float shore_k)
{
    for (int y = 0; y < ph; ++y)
        for (int x = 0; x < pw; ++x)
        {
            const std::size_t i = static_cast<std::size_t>(y) * pw + x;
            const std::uint8_t id = cover[i];
            if (!id)
                continue;
            float k = 0.0f;
            const std::uint8_t right = x + 1 < pw ? cover[i + 1] : id;
            const std::uint8_t down  = y + 1 < ph ? cover[i + pw] : id;
            if (right && right != id)
                k = std::max(k, (id == 100 || right == 100) ? shore_k : cover_k);
            if (down && down != id)
                k = std::max(k, (id == 100 || down == 100) ? shore_k : cover_k);
            if (k <= 0.0f)
                continue;
            const float m = 1.0f - k;
            buf[i] = palette::col32(
                static_cast<int>(palette::col_r(buf[i]) * m),
                static_cast<int>(palette::col_g(buf[i]) * m),
                static_cast<int>(palette::col_b(buf[i]) * m), 255);
        }
}

/// Unsharp mask: out += amount * (out − boxblur5(out)), applied only to tagged
/// pixels whose kernel reach is itself fully tagged — the lock fill and the
/// mask boundary stay byte-exact. Separable 5-tap box, run on the apron so
/// chunk edges cannot halo.
void unsharp(std::uint32_t* buf, const std::uint8_t* tag, int pw, int ph, float amount)
{
    const std::size_t n = static_cast<std::size_t>(pw) * ph;
    std::vector<float> br(n), bg(n), bb(n), tr(n), tg(n), tb(n);
    for (std::size_t i = 0; i < n; ++i)
    {
        br[i] = static_cast<float>(palette::col_r(buf[i]));
        bg[i] = static_cast<float>(palette::col_g(buf[i]));
        bb[i] = static_cast<float>(palette::col_b(buf[i]));
    }
    for (int y = 0; y < ph; ++y)
        for (int x = 0; x < pw; ++x)
        {
            const std::size_t i = static_cast<std::size_t>(y) * pw + x;
            float sr = 0, sg = 0, sb = 0;
            for (int d = -2; d <= 2; ++d)
            {
                const int xx = std::clamp(x + d, 0, pw - 1);
                const std::size_t j = static_cast<std::size_t>(y) * pw + xx;
                sr += br[j]; sg += bg[j]; sb += bb[j];
            }
            tr[i] = sr / 5.0f; tg[i] = sg / 5.0f; tb[i] = sb / 5.0f;
        }
    for (int y = 0; y < ph; ++y)
        for (int x = 0; x < pw; ++x)
        {
            const std::size_t i = static_cast<std::size_t>(y) * pw + x;
            if (!tag[i])
                continue;
            // Kernel-reach tag check: the blur below reads ±2 in both axes.
            const int xm = std::max(0, x - 2), xp = std::min(pw - 1, x + 2);
            const int ym = std::max(0, y - 2), yp = std::min(ph - 1, y + 2);
            if (!tag[static_cast<std::size_t>(y) * pw + xm]
                || !tag[static_cast<std::size_t>(y) * pw + xp]
                || !tag[static_cast<std::size_t>(ym) * pw + x]
                || !tag[static_cast<std::size_t>(yp) * pw + x])
                continue;
            float sr = 0, sg = 0, sb = 0;
            for (int d = -2; d <= 2; ++d)
            {
                const int yy = std::clamp(y + d, 0, ph - 1);
                const std::size_t j = static_cast<std::size_t>(yy) * pw + x;
                sr += tr[j]; sg += tg[j]; sb += tb[j];
            }
            const float mr = sr / 5.0f, mg = sg / 5.0f, mb = sb / 5.0f;
            buf[i] = palette::col32(
                std::clamp(static_cast<int>(br[i] + amount * (br[i] - mr) + 0.5f), 0, 255),
                std::clamp(static_cast<int>(bg[i] + amount * (bg[i] - mg) + 0.5f), 0, 255),
                std::clamp(static_cast<int>(bb[i] + amount * (bb[i] - mb) + 0.5f), 0, 255), 255);
        }
}

} // namespace

void bake_region(const bake_source& src, const geometry& g, const bake_params& p,
                 int px0, int py0, int pw, int ph, std::uint32_t* out)
{
    if (src.gw <= 0 || src.gh <= 0)
    {
        std::memset(out, 0, static_cast<std::size_t>(pw) * ph * 4u);
        return;
    }
    // APRON (BL-736): the post passes below read neighbours (ink 1 px,
    // unsharp ±2 px), so the window bakes with a margin and crops — a chunk
    // edge can then never seam or halo, because every chunk computed the same
    // overlap. Below 20 px/r no post pass runs and the apron is skipped.
    const int  A   = g.s >= 20.0 ? 6 : 0;
    const int  apw = pw + 2 * A, aph = ph + 2 * A;
    const std::size_t an = static_cast<std::size_t>(apw) * aph;
    std::vector<std::uint32_t> abuf(an);
    std::vector<std::uint8_t>  atag(an, 0u), acov(an, 0u);
    bake_window(src, g, p, px0 - A, py0 - A, apw, aph,
                abuf.data(), atag.data(), acov.data());

    // Cover-boundary ink BEFORE the stamps: a tree may straddle an inked
    // boundary and should occlude the line, never carry it.
    if (A > 0)
        edge_ink(abuf.data(), acov.data(), apw, aph, p.edge_ink, p.shore_ink);

    if (g.s >= 40.0 && p.tree_density > 0.0f)
        stamp_trees(src, g, p, px0 - A, py0 - A, apw, aph, abuf.data(), atag.data());

    // The separable near-future grade. Its haze lift falls and its contrast
    // rises with resolution (BL-736): haze is blur-adjacent, and the close
    // tiers need their local contrast more than their atmosphere.
    if (p.grade_enabled)
    {
        const float res_t = static_cast<float>(std::clamp((g.s - 24.0) / 48.0, 0.0, 1.0));
        const float lift     = p.grade_lift * (1.0f - 0.5f * res_t);
        const float contrast = p.grade_contrast + 0.045f * res_t;
        const float haze_r = 15.0f, haze_g = 15.0f, haze_b = 20.0f;
        for (std::size_t i = 0; i < an; ++i)
        {
            if (!atag[i])
                continue;
            float r_ = static_cast<float>(palette::col_r(abuf[i]));
            float g_ = static_cast<float>(palette::col_g(abuf[i]));
            float b_ = static_cast<float>(palette::col_b(abuf[i]));
            const float luma = 0.2126f * r_ + 0.7152f * g_ + 0.0722f * b_;
            r_ = r_ + (luma - r_) * p.grade_desat;
            g_ = g_ + (luma - g_) * p.grade_desat;
            b_ = b_ + (luma - b_) * p.grade_desat;
            r_ *= p.grade_cool[0]; g_ *= p.grade_cool[1]; b_ *= p.grade_cool[2];
            r_ = r_ + (haze_r - r_) * lift;
            g_ = g_ + (haze_g - g_) * lift;
            b_ = b_ + (haze_b - b_) * lift;
            r_ = (r_ - 128.0f) * contrast + 128.0f;
            g_ = (g_ - 128.0f) * contrast + 128.0f;
            b_ = (b_ - 128.0f) * contrast + 128.0f;
            abuf[i] = palette::col32(
                std::clamp(static_cast<int>(r_ + 0.5f), 0, 255),
                std::clamp(static_cast<int>(g_ + 0.5f), 0, 255),
                std::clamp(static_cast<int>(b_ + 0.5f), 0, 255), 255);
        }
    }

    // Unsharp last: it sees the graded, inked, stamped image — the thing the
    // player sees — and restores the edge contrast the interpolation lacks.
    if (g.s >= 40.0 && p.unsharp_amount > 0.0f)
        unsharp(abuf.data(), atag.data(), apw, aph, p.unsharp_amount);

    for (int y = 0; y < ph; ++y)
        std::memcpy(out + static_cast<std::size_t>(y) * pw,
                    abuf.data() + static_cast<std::size_t>(y + A) * apw + A,
                    static_cast<std::size_t>(pw) * 4u);
}

std::uint64_t region_hash(const bake_source& src, const geometry& g,
                          int px0, int py0, int pw, int ph)
{
    // FNV-1a 64 over the tile fields the bake reads, for every tile whose
    // centre could influence the window (the window rect grown by the blend
    // radius). Cheap: a few hundred tiles per chunk.
    std::uint64_t h = 14695981039346656037ull;
    auto mix = [&h](std::uint64_t v) { h ^= v; h *= 1099511628211ull; };
    if (src.gw <= 0 || src.gh <= 0)
        return h;
    const double margin = 2.0;
    const double x0 = px0 / g.s - margin,            x1 = (px0 + pw) / g.s + margin;
    const double y0 = py0 / g.s + g.y_min - margin,  y1 = (py0 + ph) / g.s + g.y_min + margin;
    const int r_lo = std::max(0, static_cast<int>(std::floor(y0 / 1.5)));
    const int r_hi = std::min(src.gh - 1, static_cast<int>(std::ceil(y1 / 1.5)));
    const int c_lo = static_cast<int>(std::floor(x0 / kSqrt3)) - 1;
    const int c_hi = static_cast<int>(std::ceil(x1 / kSqrt3)) + 1;
    for (int r = r_lo; r <= r_hi; ++r)
        for (int c = c_lo; c <= c_hi; ++c)
        {
            const int cw = ((c % src.gw) + src.gw) % src.gw;
            const std::size_t i = static_cast<std::size_t>(r) * src.gw + cw;
            mix(src.cls[i]);
            mix(src.colour[i]);
            mix(src.cover[i]);   // the feature stamps read these two, so a
            mix(src.density[i]); // cover change must move the hash
            std::uint32_t hb; static_assert(sizeof(float) == 4);
            std::memcpy(&hb, &src.height[i], 4);       mix(hb);
            std::memcpy(&hb, &src.relief_bias[i], 4);  mix(hb);
        }
    return h;
}

} // namespace ui::ground
