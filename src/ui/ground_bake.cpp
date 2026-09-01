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

geometry make_geometry(int gw, int gh, double target_px_per_r)
{
    geometry g;
    g.gw = gw;
    g.gh = gh;
    const double period = gw * kSqrt3; // canonical wrap period
    g.W = std::max(1, static_cast<int>(std::lround(period * target_px_per_r)));
    g.s = g.W / period; // re-derived so wrap copies abut exactly
    // Vertical extent: hex tops of row 0 reach y = -1, hex bottoms of the last
    // row reach 1.5 * (gh - 1) + 1.
    g.y_min = -1.0;
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

void bake_region(const bake_source& src, const geometry& g, const bake_params& p,
                 int px0, int py0, int pw, int ph, std::uint32_t* out)
{
    if (src.gw <= 0 || src.gh <= 0)
    {
        std::memset(out, 0, static_cast<std::size_t>(pw) * ph * 4u);
        return;
    }
    const double period   = g.gw * kSqrt3;
    const double R        = p.blend_radius;
    const double R2       = R * R;
    // Light from the north-west, the hillshade convention every panel-C read
    // leans on (and the same warm-up/cool-down direction the relief tint set).
    const float Lx = -0.554700196f, Ly = -0.832050323f;
    // Haze / lift target — the cool near-black the whole shell sits on (#0F0F14).
    const float haze_r = 15.0f, haze_g = 15.0f, haze_b = 20.0f;

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

            // Candidate rows: centres at 1.5 * r within blend_radius of the
            // (warped) y. Computed per pixel because the warp moves y.
            const int r_lo = std::max(0, static_cast<int>(std::ceil((y - R) / 1.5)));
            const int r_hi = std::min(src.gh - 1, static_cast<int>(std::floor((y + R) / 1.5)));

            // Gather tile-centre candidates within the blend radius. Weights
            // are a Wendland-style (R² − d²)² falloff — smooth, compact, and
            // cheap; the OWNER (nearest centre) decides the pixel's class, and
            // only candidates of that class blend, so a coastline and a mask
            // edge stay hard while everything inside a class is continuous.
            double best_d2 = 1e30;
            int    owner   = -1;
            double wsum = 0.0, cr = 0.0, cg = 0.0, cb = 0.0;
            double hsum = 0.0, gx = 0.0, gy = 0.0, rb = 0.0, jt = 0.0;
            std::uint8_t owner_cls = 0;

            // Two passes over a small candidate set: find the owner, then
            // accumulate its class. The set is at most ~3 rows × 3 cols.
            struct cand { std::size_t i; double w; };
            cand cands[24];
            int  ncand = 0;

            for (int r = r_lo; r <= r_hi; ++r)
            {
                const double cy   = 1.5 * r;
                const double dy   = y - cy;
                const double odd  = (r & 1) ? 0.5 : 0.0;
                const int    c0   = static_cast<int>(std::floor(x / kSqrt3 - odd));
                for (int dc = -1; dc <= 2; ++dc)
                {
                    const int c  = c0 + dc;
                    const double cx = kSqrt3 * (c + odd);
                    double dx = x - cx;
                    dx -= period * std::round(dx / period); // cylinder wrap
                    const double d2 = dx * dx + dy * dy;
                    if (d2 >= R2)
                        continue;
                    const int cw = ((c % src.gw) + src.gw) % src.gw;
                    const std::size_t i = static_cast<std::size_t>(r) * src.gw + cw;
                    if (src.cls[i] == static_cast<std::uint8_t>(bake_source::tile_class::void_))
                        continue;
                    const double t = R2 - d2;
                    if (ncand < 24)
                        cands[ncand++] = { i, t * t };
                    if (d2 < best_d2)
                    {
                        best_d2 = d2;
                        owner   = static_cast<int>(i);
                    }
                }
            }

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
                const float amp = p.detail_amp * (0.25f + p.landform_accent * std::fabs(bias)
                                                   + 0.35f * h);
                // The GRADIENT reads the low octave only, central-differenced
                // at half a cell: a fine octave in the slope is per-pixel
                // speckle, not terrain. The fine octave still contributes to
                // the VALUE below, where it reads as surface variation.
                const auto detail_lo = [&](double sx, double sy) -> float
                {
                    return value_noise(sx, sy, detail_cell, detail_cells, 0xD371u) - 0.5f;
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
                // gain; the detail slope is order-1 and takes amp alone.
                const float shade = (gxx * Lx + gyy * Ly) * p.relief_gain
                                  + (ddx * Lx + ddy * Ly) * amp * 0.75f;
                lum += std::clamp(shade, -0.60f, 0.60f);
                // Altitude lift, the landform's own signed bias, and the detail
                // field's own value (a crag's top is lit even side-on).
                lum += (h - 0.45f) * p.altitude_gain + bias * 0.30f + d0 * amp * 0.8f;
                // Painterly patchiness: a WHISPER of per-tile jitter (this is
                // the hex-mosaic dial — the detail field carries the texture
                // now), then fine grain.
                lum += jtt * p.jitter;
                const float n2 = value_noise(x0_, uy, noise_cell_b, noise_cells_b, 0xFAB1u);
                lum += (n2 - 0.5f) * 2.0f * p.noise_strength;
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

            if (p.grade_enabled)
            {
                // The separable near-future grade (round-2 confirmed finding):
                // desaturate, cool, lift toward the haze floor, mild contrast.
                const float luma = 0.2126f * r_ + 0.7152f * g_ + 0.0722f * b_;
                r_ = r_ + (luma - r_) * p.grade_desat;
                g_ = g_ + (luma - g_) * p.grade_desat;
                b_ = b_ + (luma - b_) * p.grade_desat;
                r_ *= p.grade_cool[0]; g_ *= p.grade_cool[1]; b_ *= p.grade_cool[2];
                r_ = r_ + (haze_r - r_) * p.grade_lift;
                g_ = g_ + (haze_g - g_) * p.grade_lift;
                b_ = b_ + (haze_b - b_) * p.grade_lift;
                r_ = (r_ - 128.0f) * p.grade_contrast + 128.0f;
                g_ = (g_ - 128.0f) * p.grade_contrast + 128.0f;
                b_ = (b_ - 128.0f) * p.grade_contrast + 128.0f;
            }

            row_out[px] = palette::col32(
                std::clamp(static_cast<int>(r_ + 0.5f), 0, 255),
                std::clamp(static_cast<int>(g_ + 0.5f), 0, 255),
                std::clamp(static_cast<int>(b_ + 0.5f), 0, 255), 255);
        }
    }
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
            std::uint32_t hb; static_assert(sizeof(float) == 4);
            std::memcpy(&hb, &src.height[i], 4);       mix(hb);
            std::memcpy(&hb, &src.relief_bias[i], 4);  mix(hb);
        }
    return h;
}

} // namespace ui::ground
