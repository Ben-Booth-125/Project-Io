#include "structure_stamps.hpp"

#include "ground_bake.hpp"
#include "terrain_palette.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

// BL-1241 (structures baked) — see structure_stamps.hpp and docs/ui/RENDERING.md
// § Installations. Everything here is pure: a function of the source snapshot,
// the geometry and the window, so two chunks overlapping one structure agree
// and every wrap copy agrees.

namespace ui::ground {

namespace {

constexpr double kSqrt3 = 1.7320508075688772;

// --- Projection constants ---------------------------------------------------
// The installation pass draws in the same axonometric the stepped tilt uses:
// ground in plan, verticals rising straight up the image. On the FLAT tiers a
// structure still stands (half height on screen) — a pure plan view of a roof
// is a coloured rectangle, and the structure has to read by silhouette. On the
// OBLIQUE tiers the verticals are pre-stretched by 1/cos(tilt) like the trees,
// so the camera squash returns them to proportion.
constexpr double k_flat_vertical  = 0.50; ///< Screen height per unit height, flat tiers.
constexpr double k_tilt_vertical  = 1.00; ///< ... at the 45-degree rung.
/// Verticals lean a touch east so a box shows its east wall as well as its
/// south one — the depth cue a pure front elevation lacks. Fraction of the
/// vertical extent; 0 would be a true front elevation.
constexpr double k_shear          = 0.20;
/// The SE drop shadow: ground offset per unit height (canonical units), the
/// direction every other pass lights from (NW).
constexpr double k_shadow_x       = 0.55;
constexpr double k_shadow_y       = 0.40;
constexpr float  k_shadow_dark    = 0.42f; ///< Ground darkening at full shadow.
constexpr float  k_ink_alpha      = 0.62f; ///< Silhouette contact line strength.
/// Tallest structure, canonical units — bounds the walk margin and the hash ring.
constexpr double k_max_height     = 1.25;

/// Deterministic lattice hash -> [0, 1). The same FNV idiom as the base bake's
/// hash01 (its own copy: that one lives in ground_bake.cpp's anonymous namespace).
float h01(int x, int y, std::uint32_t salt)
{
    std::uint32_t h = 2166136261u;
    auto mix = [&h](std::uint32_t v) { h ^= v; h *= 16777619u; h ^= h >> 13; };
    mix(static_cast<std::uint32_t>(x) * 73856093u);
    mix(static_cast<std::uint32_t>(y) * 19349663u);
    mix(salt * 83492791u);
    h ^= h >> 16;
    return static_cast<float>(h & 0x00FFFFFFu) / 16777216.0f;
}

struct rgb { float r, g, b; };
constexpr rgb operator*(rgb c, float m) { return { c.r * m, c.g * m, c.b * m }; }
constexpr rgb mix(rgb a, rgb b, float t)
{
    return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
}

// --- The house palette for structures (pre-grade: the grade desaturates and
// cools these exactly as it does the ground). No owner colour, ever.
constexpr rgb C_concrete   { 178, 172, 160 };
constexpr rgb C_concrete_d { 132, 128, 120 };
constexpr rgb C_plaster    { 200, 188, 164 };
constexpr rgb C_brick      { 150,  88,  64 };
constexpr rgb C_tile       { 170,  96,  66 };
constexpr rgb C_slate      {  88,  94, 106 };
constexpr rgb C_metal      { 128, 136, 142 };
constexpr rgb C_rust       { 140,  86,  58 };
constexpr rgb C_soot       {  58,  54,  52 };
constexpr rgb C_steel      { 152, 158, 164 };
constexpr rgb C_white      { 224, 224, 218 };
constexpr rgb C_timber     { 116,  86,  58 };
constexpr rgb C_timber_d   {  80,  60,  42 };
constexpr rgb C_log_end    { 176, 140,  96 };
constexpr rgb C_thatch     { 172, 152,  98 };
constexpr rgb C_glass      { 118, 146, 166 };
constexpr rgb C_dirt       { 128, 112,  88 };
constexpr rgb C_paved      { 150, 142, 128 };
constexpr rgb C_spoil      {  72,  66,  62 };
constexpr rgb C_stone_pale { 190, 182, 164 };
constexpr rgb C_clay       { 172, 112,  76 };
constexpr rgb C_field_a    { 160, 150,  86 };
constexpr rgb C_field_b    { 118, 132,  72 };
constexpr rgb C_field_c    { 176, 156, 100 };
constexpr rgb C_bush       {  44,  76,  40 };
constexpr rgb C_peat       {  62,  46,  34 };
constexpr rgb C_water      {  58,  92, 112 };
constexpr rgb C_olive      { 106, 110,  82 };
constexpr rgb C_khaki      { 156, 146, 110 };
constexpr rgb C_lawn       {  98, 128,  74 };
constexpr rgb C_crane      { 198, 162,  70 };
constexpr rgb C_ink        {  22,  20,  18 };
constexpr rgb C_ruin       { 116, 108,  98 };
constexpr rgb C_char       {  52,  46,  42 };

// =============================================================================
// Rasteriser — convex polygons, ellipses and capsules with ~1 px analytic AA,
// into the window buffer, honouring the tag mask.
// =============================================================================

struct raster
{
    std::uint32_t*      out = nullptr;
    const std::uint8_t* tag = nullptr;
    int pw = 0, ph = 0;
    int px0 = 0, py0 = 0;
    int W = 1;                    ///< Whole-image width: grain wraps on it.
    std::vector<float> shadow;    ///< Union of every shadow, max-combined.
};

/// A linear shade ramp across the primitive: multiplier m0 at (x0, y0), m1 at
/// (x0 + dx, y0 + dy), clamped either side. dx = dy = 0 is flat m0.
struct shade
{
    float x0 = 0, y0 = 0, dx = 0, dy = 0, m0 = 1, m1 = 1;
    float at(float x, float y) const
    {
        const float l2 = dx * dx + dy * dy;
        if (l2 <= 1e-6f)
            return m0;
        const float t = std::clamp(((x - x0) * dx + (y - y0) * dy) / l2, 0.0f, 1.0f);
        return m0 + (m1 - m0) * t;
    }
};

enum class mode : std::uint8_t { colour, ink, shadow };

/// Blend @p c into pixel (x, y) at coverage alpha @p a, with a fine painterly
/// grain keyed on the ABSOLUTE bake pixel (wrapped on W), so it never crawls.
inline void put(raster& R, int x, int y, rgb c, float a, bool grain)
{
    const std::size_t idx = static_cast<std::size_t>(y) * R.pw + x;
    if (!R.tag[idx] || a <= 0.0f)
        return;
    if (grain)
    {
        const int wx = (((R.px0 + x) % R.W) + R.W) % R.W;
        const float n = 0.94f + 0.12f * h01(wx >> 1, (R.py0 + y) >> 1, 0x5A7Bu);
        c = c * n;
    }
    std::uint32_t& dst = R.out[idx];
    const float ir = static_cast<float>(palette::col_r(dst));
    const float ig = static_cast<float>(palette::col_g(dst));
    const float ib = static_cast<float>(palette::col_b(dst));
    a = std::min(a, 1.0f);
    dst = palette::col32(
        std::clamp(static_cast<int>(ir + (c.r - ir) * a + 0.5f), 0, 255),
        std::clamp(static_cast<int>(ig + (c.g - ig) * a + 0.5f), 0, 255),
        std::clamp(static_cast<int>(ib + (c.b - ib) * a + 0.5f), 0, 255), 255);
}

/// The per-pixel sink a primitive's coverage lands in, by mode.
struct sink
{
    raster& R;
    mode    m;
    rgb     c{};
    float   alpha = 1.0f;
    shade   sh{};
    void operator()(int x, int y, float cov) const
    {
        if (cov <= 0.0f)
            return;
        if (m == mode::shadow)
        {
            float& s = R.shadow[static_cast<std::size_t>(y) * R.pw + x];
            s = std::max(s, cov * alpha);
            return;
        }
        if (m == mode::ink)
        {
            put(R, x, y, C_ink, cov * alpha, false);
            return;
        }
        put(R, x, y, c * sh.at(x + 0.5f, y + 0.5f), cov * alpha, true);
    }
};

struct pt { float x, y; };

/// Convex polygon, AA by the minimum signed edge distance. @p dil grows it by
/// that many pixels (the ink outline). Works for either winding.
void cover_poly(raster& R, const pt* p, int n, float dil, const sink& out)
{
    if (n < 3)
        return;
    float minx = p[0].x, maxx = p[0].x, miny = p[0].y, maxy = p[0].y;
    float cx = 0.0f, cy = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        minx = std::min(minx, p[i].x); maxx = std::max(maxx, p[i].x);
        miny = std::min(miny, p[i].y); maxy = std::max(maxy, p[i].y);
        cx += p[i].x; cy += p[i].y;
    }
    cx /= n; cy /= n;
    const int x0 = std::max(0, static_cast<int>(std::floor(minx - dil - 1.0f)));
    const int x1 = std::min(R.pw - 1, static_cast<int>(std::ceil(maxx + dil + 1.0f)));
    const int y0 = std::max(0, static_cast<int>(std::floor(miny - dil - 1.0f)));
    const int y1 = std::min(R.ph - 1, static_cast<int>(std::ceil(maxy + dil + 1.0f)));
    if (x0 > x1 || y0 > y1)
        return;
    // Edge normals, oriented inward by the centroid — computed about a WHOLE-PIXEL
    // local origin, so the edge constants (and so the coverage) do not depend on
    // where the window starts: translation-invariant across chunk seams.
    const float ox = std::floor(p[0].x), oy = std::floor(p[0].y);
    cx -= ox; cy -= oy;
    float nx[160], ny[160], nc[160];
    int   ne = 0;
    for (int i = 0; i < n && ne < 160; ++i)
    {
        const pt a{ p[i].x - ox, p[i].y - oy }, b{ p[(i + 1) % n].x - ox, p[(i + 1) % n].y - oy };
        const float ex = b.x - a.x, ey = b.y - a.y;
        const float len = std::sqrt(ex * ex + ey * ey);
        if (len < 1e-4f)
            continue;
        float ux = -ey / len, uy = ex / len;
        float c  = -(ux * a.x + uy * a.y);
        if (ux * cx + uy * cy + c < 0.0f)
        {
            ux = -ux; uy = -uy; c = -c;
        }
        nx[ne] = ux; ny[ne] = uy; nc[ne] = c; ++ne;
    }
    if (ne < 3)
        return;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
        {
            const float sx = (x - ox) + 0.5f, sy = (y - oy) + 0.5f;
            float d = 1e9f;
            for (int k = 0; k < ne; ++k)
                d = std::min(d, nx[k] * sx + ny[k] * sy + nc[k]);
            const float cov = std::clamp(d + 0.5f + dil, 0.0f, 1.0f);
            if (cov > 0.0f)
                out(x, y, cov);
        }
}

/// Axis ellipse, AA in pixels; @p feather > 1 softens the edge over that many px.
void cover_ellipse(raster& R, float cx, float cy, float rx, float ry, float dil, float feather,
                   const sink& out)
{
    rx += dil; ry += dil;
    if (rx <= 0.05f || ry <= 0.05f)
        return;
    const float f  = std::max(1.0f, feather);
    const int x0 = std::max(0, static_cast<int>(std::floor(cx - rx - f)));
    const int x1 = std::min(R.pw - 1, static_cast<int>(std::ceil(cx + rx + f)));
    const int y0 = std::max(0, static_cast<int>(std::floor(cy - ry - f)));
    const int y1 = std::min(R.ph - 1, static_cast<int>(std::ceil(cy + ry + f)));
    const float rm = std::min(rx, ry);
    // Sub-pixel ellipses still register, at reduced weight.
    const float wsmall = std::min(1.0f, rm * 1.6f);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
        {
            const float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
            const float dn = std::sqrt(dx * dx + dy * dy);
            const float d  = (1.0f - dn) * rm; // approx px inside the rim
            const float cov = std::clamp(d / f + 0.5f, 0.0f, 1.0f) * wsmall;
            if (cov > 0.0f)
                out(x, y, cov);
        }
}

/// Capsule (thick segment) of half-width @p hw px.
void cover_capsule(raster& R, pt a, pt b, float hw, float dil, const sink& out_)
{
    sink out = out_;
    if (hw < 0.5f)
    {
        out.alpha *= std::max(0.15f, hw * 2.0f); // a hairline is a faint line, not a thick one
        hw = 0.5f;
    }
    hw += dil;
    const int x0 = std::max(0, static_cast<int>(std::floor(std::min(a.x, b.x) - hw - 1.0f)));
    const int x1 = std::min(R.pw - 1, static_cast<int>(std::ceil(std::max(a.x, b.x) + hw + 1.0f)));
    const int y0 = std::max(0, static_cast<int>(std::floor(std::min(a.y, b.y) - hw - 1.0f)));
    const int y1 = std::min(R.ph - 1, static_cast<int>(std::ceil(std::max(a.y, b.y) + hw + 1.0f)));
    const float ex = b.x - a.x, ey = b.y - a.y;
    const float l2 = std::max(1e-6f, ex * ex + ey * ey);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
        {
            const float sx = x + 0.5f - a.x, sy = y + 0.5f - a.y;
            const float t  = std::clamp((sx * ex + sy * ey) / l2, 0.0f, 1.0f);
            const float qx = sx - ex * t, qy = sy - ey * t;
            const float d  = hw - std::sqrt(qx * qx + qy * qy);
            const float cov = std::clamp(d + 0.5f, 0.0f, 1.0f);
            if (cov > 0.0f)
                out(x, y, cov);
        }
}

/// Monotone-chain convex hull, in place; returns the hull size (<= n).
int hull(pt* p, int n)
{
    if (n < 3)
        return n;
    std::sort(p, p + n, [](const pt& a, const pt& b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
    pt h[300];
    int k = 0;
    const auto cross = [](pt o, pt a, pt b) {
        return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
    };
    for (int i = 0; i < n && k < 298; ++i)
    {
        while (k >= 2 && cross(h[k - 2], h[k - 1], p[i]) <= 0.0f) --k;
        h[k++] = p[i];
    }
    for (int i = n - 2, t = k + 1; i >= 0 && k < 298; --i)
    {
        while (k >= t && cross(h[k - 2], h[k - 1], p[i]) <= 0.0f) --k;
        h[k++] = p[i];
    }
    const int m = std::max(0, k - 1);
    std::copy(h, h + m, p);
    return m;
}

// =============================================================================
// Parts — the 3-D vocabulary every procedural form is composed of. A part is
// in CANONICAL ground units (x, y) plus height z above its structure's ground.
// =============================================================================

enum class pk : std::uint8_t
{
    box,      ///< Flat-roofed block.
    gable,    ///< Pitched roof, ridge along x.
    saw,      ///< Sawtooth-roofed hall.
    cyl,      ///< Vertical cylinder / frustum; top flat, dome or cone.
    dome,     ///< Hemispherical mound (kiln, observatory).
    heap,     ///< Conical pile (spoil, ore, stone).
    sphere,   ///< Raised spherical tank.
    beam,     ///< Thick 3-D segment (lattice members, poles, jibs).
    ring,     ///< Vertical wheel in the x-z plane (head-frame sheave).
    gell,     ///< Ground ellipse (pad, pit terrace, basin): no height, no shadow.
    grect,    ///< Ground rectangle (field strip, yard, trench): no height, no shadow.
};

enum class top : std::uint8_t { flat, dome, cone, open };

struct part
{
    pk     k   = pk::box;
    double x = 0, y = 0;   ///< Footprint centre (canonical).
    float  hw = 0, hd = 0; ///< Half extents x / y; cylinders: hw = base radius, hd = top radius.
    float  z0 = 0, h = 0;  ///< Base height and body height.
    float  rh = 0;         ///< Roof rise (gable ridge, dome, cone), or saw teeth count.
    rgb    roof{}, wall{};
    double x2 = 0, y2 = 0; ///< Beam far end.
    float  z2 = 0;
    float  w  = 0;         ///< Beam / ring thickness, canonical.
    float  alpha = 1.0f;
    float  feather = 0.0f; ///< Ground shapes: soft-edge width, canonical.
    top    cap = top::flat;
    bool   cast = true;    ///< Casts a shadow.
    bool   inked = true;   ///< Takes the silhouette contact line.
};

struct view
{
    double s = 1, y_min = 0;
    double ss = 1; ///< The bake's supersample factor (BL-1244): ink widths are in NOMINAL pixels.
    int    px0 = 0, py0 = 0;
    double vz = 0.5;   ///< Bake px per unit height = vz * s.
    double sh = 0.1;   ///< East lean px per unit height = sh * s.
};

/// One placed structure: its parts, the ground lift of its tile and a sort key.
struct instance
{
    double sort_y = 0, sort_x = 0;
    int    tie = 0;
    double lift = 0; ///< Canonical ground displacement (oblique tiers).
    std::size_t first = 0, count = 0; ///< Range into the shared part list.
};

/// Project canonical (x, y, z) to window pixels. The ABSOLUTE bake coordinate
/// is snapped to a 1/256 px grid before the window offset comes off, so a point's
/// window-relative position is exact in every chunk that draws it and two chunks
/// sharing an edge rasterise it identically (the chunk-seam rule, ground_bake_check
/// P11; found by BL-1243's variant pass).
inline pt proj(const view& v, double lift, double x, double y, double z)
{
    const double ax = std::round((x * v.s + z * v.sh * v.s) * 256.0) / 256.0;
    const double ay = std::round(((y - lift - v.y_min) * v.s - z * v.vz * v.s) * 256.0) / 256.0;
    return { static_cast<float>(ax - v.px0), static_cast<float>(ay - v.py0) };
}

/// Ground point under (x, y, z)'s shadow.
inline pt shadow_proj(const view& v, double lift, double x, double y, double z)
{
    return proj(v, lift, x + z * k_shadow_x, y + z * k_shadow_y, 0.0);
}

/// Sample a part's 3-D silhouette points (for hull-based ink and shadow).
int silhouette(const part& p, pt* out, int cap, const view& v, double lift, bool shadow_pts)
{
    int n = 0;
    const auto add = [&](double x, double y, double z) {
        if (n < cap)
            out[n++] = shadow_pts ? shadow_proj(v, lift, x, y, z) : proj(v, lift, x, y, z);
    };
    switch (p.k)
    {
        case pk::box:
        case pk::saw:
        case pk::gable:
        {
            const float zt = p.z0 + p.h;
            for (int c = 0; c < 4; ++c)
            {
                const double xx = p.x + ((c & 1) ? p.hw : -p.hw);
                const double yy = p.y + ((c & 2) ? p.hd : -p.hd);
                add(xx, yy, p.z0);
                add(xx, yy, zt);
            }
            if (p.k == pk::gable)
            {
                add(p.x - p.hw, p.y, zt + p.rh);
                add(p.x + p.hw, p.y, zt + p.rh);
            }
            if (p.k == pk::saw)
                for (int c = 0; c < 2; ++c)
                {
                    add(p.x + (c ? p.hw : -p.hw), p.y - p.hd, zt + p.hd * 0.35f);
                }
            break;
        }
        case pk::cyl:
        {
            const float zt = p.z0 + p.h;
            for (int i = 0; i < 16; ++i)
            {
                const double a = i * (6.283185307 / 16.0);
                add(p.x + p.hw * std::cos(a), p.y + p.hw * std::sin(a), p.z0);
                add(p.x + p.hd * std::cos(a), p.y + p.hd * std::sin(a), zt);
            }
            if (p.cap == top::cone)
                add(p.x, p.y, zt + p.rh);
            if (p.cap == top::dome)
                for (int i = 0; i < 8; ++i)
                {
                    const double a = i * (6.283185307 / 8.0);
                    add(p.x + p.hd * 0.7 * std::cos(a), p.y + p.hd * 0.7 * std::sin(a), zt + p.rh * 0.72);
                }
            if (p.cap == top::dome)
                add(p.x, p.y, zt + p.rh);
            break;
        }
        case pk::dome:
        case pk::heap:
        {
            for (int j = 0; j <= 4; ++j)
            {
                const double ph = j * (1.5707963 / 4.0);
                double rr, zz;
                if (p.k == pk::dome) { rr = p.hw * std::cos(ph); zz = p.z0 + p.h * std::sin(ph); }
                else                 { rr = p.hw * (1.0 - j / 4.0) * (j ? 0.85 : 1.0); zz = p.z0 + p.h * (j / 4.0); }
                const int ring_n = j == 4 ? 1 : 12;
                for (int i = 0; i < ring_n; ++i)
                {
                    const double a = i * (6.283185307 / ring_n);
                    add(p.x + rr * std::cos(a), p.y + rr * std::sin(a) * 0.95, zz);
                }
            }
            break;
        }
        case pk::sphere:
            for (int i = 0; i < 12; ++i)
            {
                const double a = i * (6.283185307 / 12.0);
                add(p.x + p.hw * std::cos(a), p.y, p.z0 + p.hw + p.hw * std::sin(a));
                add(p.x + p.hw * std::cos(a), p.y + p.hw * std::sin(a), p.z0 + p.hw);
            }
            break;
        default:
            break;
    }
    return n;
}

/// Draw one part in one mode.
void draw_part(raster& R, const view& v, double lift, const part& p, mode m)
{
    const float px_per = static_cast<float>(v.s);
    if (m == mode::shadow)
    {
        if (!p.cast)
            return;
        sink sk{ R, mode::shadow };
        sk.alpha = p.alpha;
        if (p.k == pk::beam)
        {
            const pt a = shadow_proj(v, lift, p.x, p.y, p.z0);
            const pt b = shadow_proj(v, lift, p.x2, p.y2, p.z2);
            cover_capsule(R, a, b, p.w * px_per * 0.5f, 0.0f, sk);
            return;
        }
        if (p.k == pk::ring)
        {
            const pt c = shadow_proj(v, lift, p.x, p.y, p.z0);
            sk.alpha *= 0.5f;
            cover_ellipse(R, c.x, c.y, p.hw * px_per, p.hw * px_per * 0.4f, 0.0f, 1.0f, sk);
            return;
        }
        if (p.k == pk::gell || p.k == pk::grect)
            return;
        pt pts[128];
        int n = silhouette(p, pts, 120, v, lift, true);
        // The footprint itself is under the structure: include it so the
        // shadow joins the base rather than floating off it.
        part base = p; base.h = 0; base.rh = 0;
        if (p.k == pk::sphere) { base.k = pk::cyl; base.hd = base.hw * 0.5f; base.hw *= 0.5f; base.z0 = 0; }
        const int nb = silhouette(base, pts + n, 120 - n, v, lift, false);
        // base pts are projected at z0 by silhouette(); for a raised part
        // those sit above the ground — use them only when z0 == 0.
        if (p.z0 <= 0.0f)
            n += nb;
        n = hull(pts, n);
        cover_poly(R, pts, n, 0.0f, sk);
        return;
    }

    if (m == mode::ink)
    {
        if (!p.inked)
            return;
        sink sk{ R, mode::ink };
        sk.alpha = k_ink_alpha * p.alpha;
        const float dil = 0.75f * static_cast<float>(v.ss); // one nominal ink width under a 2x bake (BL-1244)
        if (p.k == pk::beam)
        {
            cover_capsule(R, proj(v, lift, p.x, p.y, p.z0), proj(v, lift, p.x2, p.y2, p.z2),
                          p.w * px_per * 0.5f, dil * 0.6f, sk);
            return;
        }
        if (p.k == pk::ring || p.k == pk::grect)
            return;
        if (p.k == pk::gell)
        {
            const pt c = proj(v, lift, p.x, p.y, 0.0);
            cover_ellipse(R, c.x, c.y, p.hw * px_per, p.hd * px_per, dil, 1.0f, sk);
            return;
        }
        pt pts[128];
        int n = silhouette(p, pts, 128, v, lift, false);
        n = hull(pts, n);
        cover_poly(R, pts, n, dil, sk);
        return;
    }

    // --- colour ---
    sink sk{ R, mode::colour };
    sk.alpha = p.alpha;
    const auto P = [&](double x, double y, double z) { return proj(v, lift, x, y, z); };
    const auto quad = [&](pt a, pt b, pt c, pt d, rgb col, shade sh) {
        pt q[4] = { a, b, c, d };
        sink s2 = sk; s2.c = col; s2.sh = sh;
        cover_poly(R, q, 4, 0.0f, s2);
    };
    const auto vramp = [](pt top_, pt bot_, float mt, float mb) {
        shade s; s.x0 = top_.x; s.y0 = top_.y; s.dx = bot_.x - top_.x; s.dy = bot_.y - top_.y;
        s.m0 = mt; s.m1 = mb; return s;
    };
    const auto flat = [](float m) { shade s; s.m0 = s.m1 = m; return s; };

    switch (p.k)
    {
        case pk::gell:
        {
            const pt c = P(p.x, p.y, 0.0);
            sink s2 = sk; s2.c = p.roof; s2.sh = flat(1.0f);
            cover_ellipse(R, c.x, c.y, p.hw * px_per, p.hd * px_per, 0.0f,
                          std::max(1.0f, p.feather * px_per), s2);
            return;
        }
        case pk::grect:
        {
            const pt a = P(p.x - p.hw, p.y - p.hd, 0.0), b = P(p.x + p.hw, p.y - p.hd, 0.0);
            const pt c = P(p.x + p.hw, p.y + p.hd, 0.0), d = P(p.x - p.hw, p.y + p.hd, 0.0);
            quad(a, b, c, d, p.roof, flat(1.0f));
            return;
        }
        case pk::beam:
        {
            sink s2 = sk; s2.c = p.wall;
            const pt a = P(p.x, p.y, p.z0), b = P(p.x2, p.y2, p.z2);
            // Lit along its length from the NW end.
            s2.sh = vramp(a, b, 1.08f, 0.82f);
            cover_capsule(R, a, b, p.w * px_per * 0.5f, 0.0f, s2);
            return;
        }
        case pk::ring:
        {
            sink s2 = sk; s2.c = p.wall; s2.sh = flat(1.0f);
            pt prev = P(p.x + p.hw, p.y, p.z0);
            for (int i = 1; i <= 20; ++i)
            {
                const double a = i * (6.283185307 / 20.0);
                const pt cur = P(p.x + p.hw * std::cos(a), p.y, p.z0 + p.hw * std::sin(a));
                cover_capsule(R, prev, cur, p.w * px_per * 0.5f, 0.0f, s2);
                prev = cur;
            }
            return;
        }
        case pk::box:
        case pk::saw:
        case pk::gable:
        {
            const float zt = p.z0 + p.h;
            const double xl = p.x - p.hw, xr = p.x + p.hw, yb = p.y - p.hd, yf = p.y + p.hd;
            // East wall, south wall (front), then the roof over them.
            if (p.k == pk::gable)
            {
                // Back slope first: it faces the NW light.
                quad(P(xl, yb, zt), P(xr, yb, zt), P(xr, p.y, zt + p.rh), P(xl, p.y, zt + p.rh),
                     p.roof, vramp(P(xl, yb, zt), P(xr, p.y, zt + p.rh), 1.30f, 1.12f));
                pt pent[5] = { P(xr, yf, p.z0), P(xr, yb, p.z0), P(xr, yb, zt),
                               P(xr, p.y, zt + p.rh), P(xr, yf, zt) };
                sink s2 = sk; s2.c = p.wall; s2.sh = vramp(pent[4], pent[0], 0.62f, 0.48f);
                cover_poly(R, pent, 5, 0.0f, s2);
            }
            else
            {
                quad(P(xr, yf, p.z0), P(xr, yb, p.z0), P(xr, yb, zt), P(xr, yf, zt),
                     p.wall, vramp(P(xr, yf, zt), P(xr, yf, p.z0), 0.62f, 0.48f));
            }
            quad(P(xl, yf, p.z0), P(xr, yf, p.z0), P(xr, yf, zt), P(xl, yf, zt),
                 p.wall, vramp(P(xl, yf, zt), P(xl, yf, p.z0), 0.86f, 0.62f));
            if (p.k == pk::gable)
            {
                quad(P(xl, p.y, zt + p.rh), P(xr, p.y, zt + p.rh), P(xr, yf, zt), P(xl, yf, zt),
                     p.roof, vramp(P(xl, p.y, zt + p.rh), P(xr, yf, zt), 0.82f, 0.64f));
                // A lit ridge line: the crease is what makes a pitch read as one.
                if ((v.s / v.ss) * p.hd > 3.0) // nominal px (BL-1244)
                {
                    sink s2 = sk; s2.c = p.roof * 1.35f; s2.sh = flat(1.0f); s2.alpha *= 0.8f;
                    cover_capsule(R, P(xl, p.y, zt + p.rh), P(xr, p.y, zt + p.rh), 0.5f * static_cast<float>(v.ss), 0.0f, s2);
                }
                return;
            }
            quad(P(xl, yb, zt), P(xr, yb, zt), P(xr, yf, zt), P(xl, yf, zt),
                 p.roof, vramp(P(xl, yb, zt), P(xr, yf, zt), 1.14f, 0.96f));
            if (p.k == pk::saw)
            {
                // Sawtooth: north-light glazing strips + sloped roof panels,
                // back to front so each tooth overlaps the one behind it.
                const int   teeth = std::max(2, static_cast<int>(p.rh));
                const double step = (2.0 * p.hd) / teeth;
                const float th = static_cast<float>(step * 0.55);
                for (int t = 0; t < teeth; ++t)
                {
                    const double y0 = yb + step * t, y1 = y0 + step;
                    quad(P(xl, y0, zt), P(xr, y0, zt), P(xr, y0, zt + th), P(xl, y0, zt + th),
                         C_glass, flat(1.10f));
                    quad(P(xl, y0, zt + th), P(xr, y0, zt + th), P(xr, y1, zt), P(xl, y1, zt),
                         p.roof, vramp(P(xl, y0, zt + th), P(xl, y1, zt), 1.00f, 0.80f));
                }
            }
            return;
        }
        case pk::cyl:
        {
            const float zt = p.z0 + p.h;
            pt pts[40];
            int n = 0;
            for (int i = 0; i < 16; ++i)
            {
                const double a = i * (6.283185307 / 16.0);
                pts[n++] = P(p.x + p.hw * std::cos(a), p.y + p.hw * std::sin(a), p.z0);
                pts[n++] = P(p.x + p.hd * std::cos(a), p.y + p.hd * std::sin(a), zt);
            }
            n = hull(pts, n);
            const pt lft = P(p.x - p.hw, p.y, p.z0), rgt = P(p.x + p.hw, p.y, p.z0);
            shade body; body.x0 = lft.x; body.y0 = lft.y; body.dx = rgt.x - lft.x; body.dy = 0;
            body.m0 = 1.12f; body.m1 = 0.52f;
            sink s2 = sk; s2.c = p.wall; s2.sh = body;
            cover_poly(R, pts, n, 0.0f, s2);
            const pt tc = P(p.x, p.y, zt);
            const float tr = p.hd * px_per;
            if (p.cap == top::flat || p.cap == top::open)
            {
                sink s3 = sk; s3.c = p.cap == top::open ? C_soot : p.roof;
                shade ts; ts.x0 = tc.x - tr; ts.y0 = tc.y - tr; ts.dx = 2 * tr; ts.dy = 2 * tr;
                ts.m0 = p.cap == top::open ? 0.8f : 1.16f; ts.m1 = p.cap == top::open ? 1.1f : 0.92f;
                s3.sh = ts;
                cover_ellipse(R, tc.x, tc.y, tr, tr, 0.0f, 1.0f, s3);
                if (p.cap == top::open && tr > 2.0f)
                {
                    // A lip ring so an open cooling-tower mouth reads as a rim.
                    sink s4 = sk; s4.c = p.wall; s4.sh = flat(1.05f);
                    s4.alpha *= 0.85f;
                    cover_ellipse(R, tc.x, tc.y, tr, tr, 0.0f, 1.0f, s4);
                    sink s5 = sk; s5.c = C_soot; s5.sh = flat(0.75f);
                    cover_ellipse(R, tc.x, tc.y + tr * 0.06f, tr * 0.82f, tr * 0.82f, 0.0f, 1.0f, s5);
                }
            }
            else
            {
                pt cp[40];
                int m2 = 0;
                for (int i = 0; i < 16; ++i)
                {
                    const double a = i * (6.283185307 / 16.0);
                    cp[m2++] = P(p.x + p.hd * std::cos(a), p.y + p.hd * std::sin(a), zt);
                }
                if (p.cap == top::cone)
                    cp[m2++] = P(p.x, p.y, zt + p.rh);
                else
                    for (int i = 0; i < 12; ++i)
                    {
                        const double a = i * (6.283185307 / 12.0);
                        cp[m2++] = P(p.x + p.hd * 0.72 * std::cos(a), p.y + p.hd * 0.72 * std::sin(a),
                                     zt + p.rh * 0.70);
                    }
                if (p.cap == top::dome)
                    cp[m2++] = P(p.x, p.y, zt + p.rh);
                m2 = hull(cp, m2);
                const pt ap = P(p.x - p.hd, p.y - p.hd, zt + p.rh);
                const pt bp = P(p.x + p.hd, p.y + p.hd, zt);
                shade cs; cs.x0 = ap.x; cs.y0 = ap.y; cs.dx = bp.x - ap.x; cs.dy = bp.y - ap.y;
                cs.m0 = 1.24f; cs.m1 = 0.70f;
                sink s3 = sk; s3.c = p.roof; s3.sh = cs;
                cover_poly(R, cp, m2, 0.0f, s3);
            }
            return;
        }
        case pk::dome:
        case pk::heap:
        {
            pt pts[128];
            int n = silhouette(p, pts, 128, v, lift, false);
            n = hull(pts, n);
            const pt ap = P(p.x - p.hw, p.y - p.hw, p.z0 + p.h);
            const pt bp = P(p.x + p.hw, p.y + p.hw, p.z0);
            shade cs; cs.x0 = ap.x; cs.y0 = ap.y; cs.dx = bp.x - ap.x; cs.dy = bp.y - ap.y;
            cs.m0 = p.k == pk::dome ? 1.30f : 1.22f; cs.m1 = 0.58f;
            sink s2 = sk; s2.c = p.roof; s2.sh = cs;
            cover_poly(R, pts, n, 0.0f, s2);
            return;
        }
        case pk::sphere:
        {
            const pt c = P(p.x, p.y, p.z0 + p.hw);
            const float r = p.hw * px_per;
            sink s2 = sk; s2.c = p.roof;
            shade cs; cs.x0 = c.x - r * 0.8f; cs.y0 = c.y - r * 0.8f; cs.dx = r * 1.6f; cs.dy = r * 1.6f;
            cs.m0 = 1.30f; cs.m1 = 0.60f;
            s2.sh = cs;
            cover_ellipse(R, c.x, c.y, r, r, 0.0f, 1.0f, s2);
            return;
        }
    }
}

// =============================================================================
// Forms — one procedural form per depicted subject. A form is authored in
// LOCAL units (the slot radius = 1, heights in the same unit) and placed by a
// builder that scales by the slot size and mirrors by the stamp's hash.
// =============================================================================

struct builder
{
    std::vector<part>& out;
    double ax = 0, ay = 0; ///< Anchor (canonical).
    float  k  = 0.5f;      ///< Slot size: local unit -> canonical.
    float  mx = 1.0f;      ///< +1 / -1: the hash mirror.
    bool   fine = true;    ///< Resolution admits fine members (lattice braces, racks).

    static float f(double v) { return static_cast<float>(v); }

    part& add(pk kind, double x, double y)
    {
        part p;
        p.k = kind;
        p.x = ax + x * mx * k;
        p.y = ay + y * k;
        out.push_back(p);
        return out.back();
    }
    void box(double x, double y, double hw, double hd, double h, rgb roof, rgb wall, double z0 = 0)
    {
        part& p = add(pk::box, x, y);
        p.hw = f(hw * k); p.hd = f(hd * k); p.h = f(h * k); p.z0 = f(z0 * k); p.roof = roof; p.wall = wall;
    }
    void gable(double x, double y, double hw, double hd, double h, double rh, rgb roof, rgb wall)
    {
        part& p = add(pk::gable, x, y);
        p.hw = f(hw * k); p.hd = f(hd * k); p.h = f(h * k); p.rh = f(rh * k); p.roof = roof; p.wall = wall;
    }
    void saw(double x, double y, double hw, double hd, double h, int teeth, rgb roof, rgb wall)
    {
        part& p = add(pk::saw, x, y);
        p.hw = f(hw * k); p.hd = f(hd * k); p.h = f(h * k); p.rh = f(static_cast<float>(teeth));
        p.roof = roof; p.wall = wall;
    }
    void cyl(double x, double y, double r, double h, top cap, double rh, rgb roof, rgb wall,
             double z0 = 0, double r_top = -1)
    {
        part& p = add(pk::cyl, x, y);
        p.hw = f(r * k); p.hd = f((r_top < 0 ? r : r_top) * k); p.h = f(h * k); p.z0 = f(z0 * k);
        p.rh = f(rh * k); p.cap = cap; p.roof = roof; p.wall = wall;
    }
    void dome(double x, double y, double r, double h, rgb col)
    {
        part& p = add(pk::dome, x, y);
        p.hw = f(r * k); p.h = f(h * k); p.roof = col;
    }
    void heap(double x, double y, double r, double h, rgb col)
    {
        part& p = add(pk::heap, x, y);
        p.hw = f(r * k); p.h = f(h * k); p.roof = col;
    }
    void sphere(double x, double y, double z0, double r, rgb col)
    {
        part& p = add(pk::sphere, x, y);
        p.hw = f(r * k); p.z0 = f(z0 * k); p.roof = col;
    }
    void beam(double x1, double y1, double z1, double x2, double y2, double z2, double w, rgb col,
              bool essential = true)
    {
        if (!essential && !fine)
            return;
        part& p = add(pk::beam, x1, y1);
        p.z0 = f(z1 * k);
        p.x2 = ax + x2 * mx * k; p.y2 = ay + y2 * k; p.z2 = f(z2 * k);
        p.w = f(w * k); p.wall = col;
    }
    void ring(double x, double y, double z, double r, double w, rgb col)
    {
        part& p = add(pk::ring, x, y);
        p.z0 = f(z * k); p.hw = f(r * k); p.w = f(w * k); p.wall = col;
    }
    void gell(double x, double y, double rx, double ry, rgb col, double alpha, double feather = 0.0f)
    {
        part& p = add(pk::gell, x, y);
        p.hw = f(rx * k); p.hd = f(ry * k); p.roof = col; p.alpha = f(alpha); p.feather = f(feather * k);
        p.cast = false; p.inked = false;
    }
    void grect(double x, double y, double hw, double hd, rgb col, double alpha)
    {
        part& p = add(pk::grect, x, y);
        p.hw = f(hw * k); p.hd = f(hd * k); p.roof = col; p.alpha = f(alpha);
        p.cast = false; p.inked = false;
    }
    /// A four-legged lattice tower from footprint half-width @p b to @p t at height @p h.
    void lattice(double x, double y, double b, double t, double h, int bays, double w, rgb col)
    {
        beam(x - b, y, 0, x - t, y, h, w, col);
        beam(x + b, y, 0, x + t, y, h, w, col);
        for (int i = 1; i <= bays; ++i)
        {
            const double z  = h * i / bays;
            const double zp = h * (i - 1) / bays;
            const double wi = b + (t - b) * (z / h), wp = b + (t - b) * (zp / h);
            beam(x - wi, y, z, x + wi, y, z, w * 0.7f, col, i == bays);
            beam(x - wp, y, zp, x + wi, y, z, w * 0.55f, col, false);
        }
    }
};

/// The house dirt / paving / lawn pads and pits are ground; everything else stands.

void form_extraction(builder& b, extraction_family f, std::uint32_t seed)
{
    switch (f)
    {
        case extraction_family::mine:
            b.heap(0.55, -0.40, 0.42, 0.42, C_spoil);
            b.gable(-0.45, 0.05, 0.34, 0.22, 0.30, 0.14, C_metal, C_brick);
            // Head-frame: twin legs over the shaft, a back-stay to the winding
            // house, the sheave wheel on top.
            b.beam(-0.30, 0.15, 0.0, 0.05, 0.22, 1.30, 0.07, C_rust);
            b.lattice(0.12, 0.25, 0.22, 0.07, 1.40, 3, 0.07, C_rust);
            b.box(0.12, 0.25, 0.12, 0.08, 0.06, C_soot, C_rust, 1.40f);
            b.ring(0.12, 0.25, 1.52f, 0.16f, 0.05f, C_soot);
            b.box(0.52, 0.48, 0.13, 0.10, 0.20, C_soot, C_timber_d);
            break;
        case extraction_family::quarry:
        {
            const rgb st = mix(C_stone_pale, C_clay, (seed & 1) ? 0.0f : 0.15f);
            b.gell(0.0, 0.0, 0.92f, 0.70f, st * 0.96f, 0.95f, 0.06f);
            b.gell(0.04, 0.05, 0.74f, 0.54f, st * 0.80f, 1.0f);
            b.gell(0.07, 0.08, 0.56f, 0.40f, st * 0.94f, 0.9f);
            b.gell(0.09, 0.10, 0.52f, 0.36f, st * 0.66f, 1.0f);
            b.gell(0.11, 0.12, 0.34f, 0.23f, st * 0.78f, 0.9f);
            b.gell(0.12, 0.13, 0.30f, 0.20f, st * 0.52f, 1.0f);
            b.gable(-0.72, 0.62, 0.18, 0.12, 0.14, 0.07, C_metal, C_timber);
            b.box(0.62, 0.62, 0.10, 0.08, 0.10, st * 1.05f, st * 0.85f);
            b.box(0.80, 0.70, 0.08, 0.07, 0.08, st * 1.05f, st * 0.85f);
            break;
        }
        case extraction_family::oil_well:
            b.cyl(-0.60, -0.35, 0.22, 0.30, top::flat, 0, C_white, C_white * 0.92f);
            b.cyl(0.62, -0.30, 0.20, 0.26, top::flat, 0, C_white, C_white * 0.92f);
            b.lattice(0.0, 0.05, 0.26, 0.05, 1.80, 4, 0.06, C_steel * 0.8f);
            b.box(0.0, 0.05, 0.07, 0.05, 0.06, C_soot, C_steel, 1.80f);
            b.box(0.0, 0.38, 0.30, 0.10, 0.10, C_steel, C_metal * 0.8f);
            b.box(0.55, 0.50, 0.15, 0.10, 0.16, C_metal, C_concrete_d);
            break;
        case extraction_family::water_well:
            // Water tower behind, then the well housing — the read the spec names.
            b.beam(0.40, -0.30, 0.0, 0.46, -0.30, 0.95, 0.06, C_steel * 0.8f);
            b.beam(0.70, -0.30, 0.0, 0.64, -0.30, 0.95, 0.06, C_steel * 0.8f);
            b.cyl(0.55, -0.30, 0.20, 0.30, top::cone, 0.10, C_slate, C_steel, 0.95f);
            b.gell(-0.10, 0.12, 0.46f, 0.40f, C_water * 0.9f, 0.55f, 0.08f);
            b.cyl(-0.10, 0.10, 0.30, 0.30, top::cone, 0.28, C_slate, C_plaster);
            b.box(-0.55, 0.52, 0.14, 0.10, 0.16, C_tile, C_plaster);
            break;
        case extraction_family::timber:
            b.gable(0.05, -0.30, 0.55, 0.22, 0.22, 0.12, C_metal, C_timber);
            b.box(-0.40, 0.30, 0.34, 0.11, 0.12, C_log_end, C_timber);
            b.box(-0.40, 0.52, 0.30, 0.10, 0.10, C_log_end, C_timber);
            b.box(0.40, 0.42, 0.30, 0.12, 0.13, C_log_end, C_timber * 0.9f);
            break;
        case extraction_family::farm:
        {
            const bool across = (seed >> 3) & 1;
            for (int s = -3; s <= 3; ++s)
            {
                const rgb c = (s & 1) ? C_field_a : ((s % 3 == 0) ? C_field_b : C_field_c);
                if (across) b.grect(0.10, s * 0.25, 0.95f, 0.11f, c, 0.62f);
                else        b.grect(s * 0.27, 0.10, 0.12f, 0.92f, c, 0.62f);
            }
            b.cyl(0.25, -0.22, 0.13, 0.62, top::dome, 0.10, C_steel, C_concrete);
            b.gable(-0.20, 0.0, 0.32, 0.21, 0.22, 0.17, C_slate, C_brick * 0.95f);
            break;
        }
        case extraction_family::plantation:
            for (int r = -3; r <= 3; ++r)
                for (int c = -4; c <= 4; ++c)
                {
                    const double x = c * 0.21, y = r * 0.24;
                    if (std::fabs(x) < 0.42 && std::fabs(y + 0.05) < 0.30)
                        continue; // the shed's yard
                    if (x * x + y * y > 0.95)
                        continue;
                    b.sphere(x, y, 0.0f, 0.075f, C_bush);
                }
            b.gable(0.0, -0.05, 0.32, 0.18, 0.16, 0.12, C_thatch, C_timber);
            break;
        case extraction_family::peat:
            for (int s = -3; s <= 2; ++s)
                b.grect(0.05, s * 0.24 + 0.05, 0.78f, 0.055f, C_peat, 0.85f);
            b.heap(0.45, 0.55, 0.14, 0.12, C_peat * 1.4f);
            b.heap(0.70, 0.45, 0.12, 0.10, C_peat * 1.4f);
            b.gable(-0.55, -0.55, 0.20, 0.14, 0.14, 0.10, C_thatch, C_timber_d);
            break;
        case extraction_family::trapping:
            b.gable(-0.15, -0.05, 0.32, 0.20, 0.18, 0.15, C_timber_d * 1.15f, C_timber_d);
            for (int r = 0; r < 2; ++r)
            {
                const double y = 0.30 + r * 0.25, x = 0.50 - r * 0.15;
                b.beam(x - 0.18, y, 0.0, x - 0.18, y, 0.30, 0.04, C_timber_d);
                b.beam(x + 0.18, y, 0.0, x + 0.18, y, 0.30, 0.04, C_timber_d);
                b.beam(x - 0.20, y, 0.30, x + 0.20, y, 0.30, 0.04, C_timber_d);
                b.box(x - 0.07, y, 0.06, 0.01, 0.16, C_log_end, C_log_end * 0.9f, 0.12f);
                b.box(x + 0.08, y, 0.06, 0.01, 0.14, C_log_end, C_log_end * 0.85f, 0.14f);
            }
            break;
        default:
            break;
    }
}

void form_processing(builder& b, processing_family f, std::uint32_t /*seed*/)
{
    switch (f)
    {
        case processing_family::metal_foundry:
            b.cyl(-0.45, -0.40, 0.07, 1.30, top::open, 0, C_soot, C_brick * 0.85f);
            b.cyl(-0.22, -0.42, 0.07, 1.15, top::open, 0, C_soot, C_brick * 0.85f);
            b.cyl(0.52, -0.25, 0.19, 0.78, top::cone, 0.22, C_soot, C_rust);
            b.gable(-0.15, -0.02, 0.56, 0.30, 0.26, 0.24, C_rust * 0.85f, C_brick * 0.75f);
            b.heap(0.55, 0.48, 0.20, 0.15, C_rust * 0.8f);
            break;
        case processing_family::fuel_production:
            b.box(-0.10, -0.45, 0.40, 0.10, 0.14, C_log_end, C_timber);
            b.cyl(0.55, -0.40, 0.05, 0.62, top::open, 0, C_soot, C_brick);
            b.dome(-0.48, 0.08, 0.21, 0.26, C_brick * 1.05f);
            b.dome(0.00, 0.14, 0.23, 0.29, C_brick * 1.05f);
            b.dome(0.48, 0.08, 0.21, 0.26, C_brick * 1.05f);
            b.heap(0.10, 0.55, 0.18, 0.10, C_soot);
            break;
        case processing_family::construction_materials:
            b.box(0.45, -0.38, 0.07, 0.07, 1.20, C_soot, C_brick);
            b.gable(-0.05, -0.08, 0.60, 0.22, 0.20, 0.17, C_tile, C_brick);
            for (int i = 0; i < 3; ++i)
                b.box(-0.45 + i * 0.30, 0.45, 0.10, 0.08, 0.12, C_tile * 1.1f, C_tile * 0.9f);
            b.heap(0.62, 0.45, 0.18, 0.13, C_clay);
            break;
        case processing_family::construction:
            b.grect(0.0, 0.10, 0.80f, 0.55f, C_concrete * 0.9f, 0.55f);
            b.gable(-0.45, -0.40, 0.30, 0.16, 0.20, 0.10, C_metal, C_concrete_d);
            b.box(0.20, 0.05, 0.22, 0.06, 0.08, C_steel, C_steel * 0.8f);
            b.box(0.20, 0.22, 0.22, 0.06, 0.08, C_log_end, C_timber);
            b.box(-0.30, 0.35, 0.16, 0.12, 0.10, C_concrete, C_concrete_d);
            // Gantry: two legs and a beam — a fixed yard crane, not the
            // construction-site tower crane (which means "being built").
            b.beam(-0.05, 0.45, 0.0, -0.05, 0.45, 0.62, 0.07, C_steel * 0.7f);
            b.beam(0.62, 0.45, 0.0, 0.62, 0.45, 0.62, 0.07, C_steel * 0.7f);
            b.beam(-0.10, 0.45, 0.62, 0.67, 0.45, 0.62, 0.08, C_steel * 0.7f);
            break;
        case processing_family::artisan_goods:
            b.grect(0.0, 0.0, 0.62f, 0.50f, C_paved, 0.55f);
            b.gable(-0.40, -0.30, 0.26, 0.17, 0.18, 0.13, C_tile, C_plaster);
            b.gable(0.36, -0.32, 0.24, 0.16, 0.20, 0.12, C_tile * 0.9f, C_plaster * 0.95f);
            b.gable(0.00, 0.32, 0.30, 0.17, 0.18, 0.13, C_slate, C_plaster);
            break;
        case processing_family::food_processing:
            for (int i = 0; i < 3; ++i)
                b.cyl(-0.34 + i * 0.30, -0.32, 0.14, 0.78, top::dome, 0.10, C_steel, C_concrete);
            b.box(0.0, 0.22, 0.56, 0.22, 0.28, C_metal, C_plaster);
            break;
        case processing_family::chemical_works:
            b.cyl(0.55, -0.42, 0.05, 0.95, top::open, 0, C_soot, C_steel);
            b.beam(-0.50, -0.30, 0.0, -0.50, -0.30, 0.10, 0.08, C_steel * 0.7f);
            b.sphere(-0.50, -0.30, 0.10f, 0.24f, C_white);
            b.beam(0.02, -0.25, 0.0, 0.02, -0.25, 0.10, 0.08, C_steel * 0.7f);
            b.sphere(0.02, -0.25, 0.10f, 0.20f, C_white);
            b.beam(-0.75, 0.12, 0.20, 0.75, 0.12, 0.20, 0.05, C_rust);
            b.beam(-0.75, 0.16, 0.12, 0.75, 0.16, 0.12, 0.04, C_steel * 0.8f);
            b.box(0.25, 0.42, 0.34, 0.14, 0.20, C_metal, C_concrete);
            break;
        case processing_family::refinery:
            b.cyl(0.58, -0.45, 0.035, 1.30, top::flat, 0, C_soot, C_steel * 0.8f);
            b.cyl(-0.22, -0.22, 0.08, 1.05, top::cone, 0.06, C_steel, C_steel);
            b.cyl(0.02, -0.26, 0.10, 1.35, top::cone, 0.07, C_steel, C_steel);
            b.cyl(0.24, -0.18, 0.07, 0.90, top::cone, 0.05, C_steel, C_steel);
            b.beam(-0.70, 0.10, 0.25, 0.70, 0.10, 0.25, 0.05, C_rust);
            b.cyl(-0.52, 0.42, 0.25, 0.22, top::flat, 0, C_white, C_white * 0.9f);
            b.cyl(0.08, 0.50, 0.25, 0.22, top::flat, 0, C_white, C_white * 0.9f);
            b.cyl(0.62, 0.40, 0.20, 0.20, top::flat, 0, C_white, C_white * 0.9f);
            break;
        case processing_family::power_generation:
            b.cyl(0.62, -0.42, 0.05, 1.40, top::open, 0, C_soot, C_concrete);
            b.cyl(-0.28, -0.18, 0.40, 0.95, top::open, 0, C_soot, C_concrete * 1.05f, 0.0f, 0.29f);
            b.box(0.38, 0.28, 0.34, 0.22, 0.36, C_slate, C_concrete_d);
            break;
        case processing_family::electronics:
            b.box(0.0, 0.0, 0.62, 0.40, 0.12, C_concrete, C_glass);
            b.box(0.0, 0.0, 0.62, 0.40, 0.20, C_white, C_white * 0.92f, 0.12f);
            b.box(-0.30, -0.15, 0.08, 0.06, 0.06, C_steel, C_steel * 0.8f, 0.32f);
            b.box(0.10, -0.12, 0.10, 0.07, 0.07, C_steel, C_steel * 0.8f, 0.32f);
            b.box(0.38, -0.18, 0.07, 0.06, 0.05, C_steel, C_steel * 0.8f, 0.32f);
            break;
        case processing_family::advanced_fabrication:
            b.saw(-0.05, -0.08, 0.66, 0.40, 0.30, 4, C_metal, C_concrete);
            b.box(0.58, 0.48, 0.18, 0.12, 0.40, C_concrete, C_glass);
            break;
        case processing_family::welfare_goods:
            b.cyl(-0.48, -0.42, 0.05, 0.80, top::open, 0, C_soot, C_brick);
            b.beam(0.40, -0.40, 0.0, 0.44, -0.40, 0.85, 0.05, C_steel * 0.8f);
            b.beam(0.66, -0.40, 0.0, 0.62, -0.40, 0.85, 0.05, C_steel * 0.8f);
            b.cyl(0.53, -0.40, 0.17, 0.26, top::cone, 0.08, C_slate, C_steel, 0.85f);
            b.box(-0.05, 0.08, 0.52, 0.30, 0.28, C_metal, C_brick);
            break;
        case processing_family::general:
        default:
            b.cyl(0.45, -0.40, 0.06, 0.95, top::open, 0, C_soot, C_brick);
            b.gable(-0.05, 0.0, 0.52, 0.28, 0.22, 0.22, C_metal, C_plaster * 0.9f);
            break;
    }
}

/// Port: quay + jetty toward the water, warehouse inland, a gantry on the quay.
/// (@p wx, @p wy) is the unit water direction on a cardinal axis.
void form_port(builder& b, int wx, int wy)
{
    // Author in a frame where the water is +u; map (u, v) to (x, y).
    const auto X = [&](double u, double v) { return wx != 0 ? u * wx : v; };
    const auto Y = [&](double u, double v) { return wx != 0 ? v : u * wy; };
    const auto HX = [&](float hu, float hv) { return wx != 0 ? hu : hv; };
    const auto HY = [&](float hu, float hv) { return wx != 0 ? hv : hu; };
    const float saved = b.mx;
    b.mx = 1.0f; // the water side fixes the layout; no hash mirror
    // Back-to-front order depends on orientation: build a small list and sort by y.
    std::vector<part> local;
    builder lb{ local, b.ax, b.ay, b.k, 1.0f, b.fine };
    lb.box(X(0.30, 0.0), Y(0.30, 0.0), HX(0.28f, 0.70f), HY(0.28f, 0.70f), 0.05f, C_concrete, C_concrete_d);
    lb.box(X(0.95, 0.12), Y(0.95, 0.12), HX(0.42f, 0.07f), HY(0.42f, 0.07f), 0.04f, C_timber * 1.2f, C_timber_d);
    lb.gable(X(-0.30, -0.15), Y(-0.30, -0.15), HX(0.20f, 0.42f), HY(0.20f, 0.42f), 0.26f, 0.10f,
             C_metal, C_brick);
    lb.box(X(0.28, 0.38), Y(0.28, 0.38), HX(0.08f, 0.10f), HY(0.08f, 0.10f), 0.10f, C_rust, C_rust * 0.8f, 0.05f);
    lb.box(X(0.22, -0.40), Y(0.22, -0.40), HX(0.09f, 0.12f), HY(0.09f, 0.12f), 0.10f, C_steel, C_steel * 0.8f, 0.05f);
    // Gantry crane on the quay edge, boom over the water.
    lb.beam(X(0.40, 0.20), Y(0.40, 0.20), 0.05f, X(0.40, 0.20), Y(0.40, 0.20), 0.85f, 0.06f, C_crane * 0.85f);
    lb.beam(X(0.40, 0.20), Y(0.40, 0.20), 0.85f, X(0.95, 0.20), Y(0.95, 0.20), 0.70f, 0.05f, C_crane * 0.85f);
    lb.beam(X(0.40, 0.20), Y(0.40, 0.20), 0.85f, X(0.10, 0.20), Y(0.10, 0.20), 0.75f, 0.05f, C_crane * 0.85f);
    std::stable_sort(local.begin(), local.end(), [](const part& a, const part& c) {
        const auto front = [](const part& p) { return p.k == pk::beam ? std::max(p.y, p.y2) : p.y + p.hd; };
        return front(a) < front(c);
    });
    for (const part& p : local)
        b.out.push_back(p);
    b.mx = saved;
}

void form_other(builder& b, building_type t, std::uint32_t seed)
{
    switch (t)
    {
        case building_type::launchpad:
            b.grect(0.0, 0.05, 0.85f, 0.72f, C_concrete, 0.85f);
            b.gell(-0.05, 0.05, 0.30f, 0.22f, C_soot, 0.9f);
            b.lattice(0.28, -0.20, 0.12, 0.08, 1.55, 5, 0.06, C_rust);
            b.beam(0.20, -0.20, 1.10, -0.02, -0.15, 1.10, 0.05, C_rust);
            b.cyl(-0.08, -0.15, 0.09, 1.30, top::cone, 0.28, C_white, C_white);
            b.box(-0.60, 0.55, 0.15, 0.10, 0.12, C_concrete_d, C_concrete_d * 0.8f);
            break;
        case building_type::inland_logistics_hub:
        {
            b.grect(0.05, 0.25, 0.80f, 0.45f, C_concrete * 0.88f, 0.6f);
            b.gable(-0.30, -0.45, 0.42, 0.16, 0.22, 0.08, C_metal, C_concrete_d);
            b.gable(0.48, -0.40, 0.30, 0.16, 0.22, 0.08, C_metal * 0.92f, C_concrete_d);
            const rgb boxes[4] = { C_rust, C_steel * 0.8f, { 92, 110, 120 }, { 140, 120, 70 } };
            for (int r = 0; r < 2; ++r)
                for (int c = 0; c < 4; ++c)
                {
                    const rgb col = boxes[(c + r + seed) & 3];
                    const float h = ((c * 7 + r * 3 + seed) % 3 == 0) ? 0.18f : 0.10f;
                    b.box(-0.45 + c * 0.27, 0.12 + r * 0.22, 0.11, 0.07, h, col * 1.05f, col * 0.8f);
                }
            b.beam(-0.70, 0.20, 0.0, -0.70, 0.20, 0.55, 0.06, C_crane * 0.8f);
            b.beam(0.62, 0.20, 0.0, 0.62, 0.20, 0.55, 0.06, C_crane * 0.8f);
            b.beam(-0.75, 0.20, 0.55, 0.67, 0.20, 0.55, 0.07, C_crane * 0.8f);
            break;
        }
        case building_type::military_base:
            b.grect(0.0, 0.0, 0.78f, 0.70f, C_khaki, 0.55f);
            b.box(0.0, -0.75, 0.82, 0.03, 0.10, C_concrete_d, C_concrete_d * 0.8f);
            b.box(-0.80, 0.0, 0.03, 0.75, 0.10, C_concrete_d, C_concrete_d * 0.8f);
            b.box(0.80, 0.0, 0.03, 0.75, 0.10, C_concrete_d, C_concrete_d * 0.8f);
            b.gable(-0.32, -0.42, 0.30, 0.12, 0.16, 0.08, C_olive, C_khaki * 0.85f);
            b.gable(0.32, -0.42, 0.30, 0.12, 0.16, 0.08, C_olive, C_khaki * 0.85f);
            b.gable(-0.32, -0.08, 0.30, 0.12, 0.16, 0.08, C_olive, C_khaki * 0.85f);
            b.gable(0.32, -0.08, 0.30, 0.12, 0.16, 0.08, C_olive, C_khaki * 0.85f);
            b.box(0.62, 0.55, 0.08, 0.08, 0.55, C_olive, C_concrete_d);
            b.box(0.0, 0.75, 0.82, 0.03, 0.10, C_concrete_d, C_concrete_d * 0.8f);
            break;
        case building_type::research_institute:
            b.box(-0.22, 0.10, 0.46, 0.28, 0.30, C_white, C_glass);
            b.cyl(0.45, -0.30, 0.22, 0.24, top::dome, 0.22, { 206, 210, 214 }, C_concrete);
            break;
        case building_type::schooling:
            b.grect(0.0, 0.45, 0.55f, 0.20f, C_paved, 0.6f);
            b.gable(-0.10, -0.05, 0.42, 0.24, 0.22, 0.16, C_slate, C_plaster);
            b.cyl(0.48, -0.15, 0.08, 0.62, top::cone, 0.22, C_slate, C_plaster);
            break;
        case building_type::university:
            b.grect(0.0, 0.0, 0.40f, 0.38f, C_lawn, 0.8f);
            b.box(0.0, -0.55, 0.66, 0.15, 0.32, C_slate, C_stone_pale);
            b.cyl(0.0, -0.55, 0.12, 0.62, top::dome, 0.16, { 150, 160, 150 }, C_stone_pale);
            b.box(-0.56, 0.0, 0.14, 0.40, 0.28, C_slate, C_stone_pale);
            b.box(0.56, 0.0, 0.14, 0.40, 0.28, C_slate, C_stone_pale);
            b.box(0.0, 0.55, 0.42, 0.12, 0.22, C_slate, C_stone_pale);
            break;
        default:
            b.gable(0.0, 0.0, 0.45, 0.26, 0.26, 0.12, C_metal, C_concrete);
            break;
    }
}

/// A site under construction: a low raw base, a scaffold cage, a tower crane.
void form_scaffold(builder& b, std::uint32_t /*seed*/)
{
    const float hw = 0.52f, hd = 0.30f, top = 0.48f;
    b.grect(0.0, 0.05, 0.70f, 0.48f, C_dirt * 0.9f, 0.6f);
    b.box(0.0, 0.0, hw, hd, 0.12, C_concrete_d, C_concrete * 0.9f);
    const rgb pole { 176, 170, 152 };
    // Back face first, then the front cage over it.
    for (int face = 0; face < 2; ++face)
    {
        const double y = face == 0 ? -hd : hd;
        for (int i = 0; i <= 3; ++i)
        {
            const double x = -hw + i * (2.0 * hw / 3.0);
            b.beam(x, y, 0.0, x, y, top, 0.035, pole);
        }
        for (int l = 1; l <= 3; ++l)
            b.beam(-hw, y, top * l / 3.0f, hw, y, top * l / 3.0f, 0.03, pole, l == 3 || face == 1);
        if (face == 1)
            b.beam(-hw, y, 0.0, hw, y, top, 0.025, pole, false);
    }
    // Tower crane: mast at the back right, jib across the site.
    b.beam(0.62, -0.42, 0.0, 0.62, -0.42, 1.35, 0.06, C_crane);
    b.beam(-0.45, -0.42, 1.30, 0.86, -0.42, 1.30, 0.05, C_crane);
    b.box(0.80, -0.42, 0.07, 0.05, 0.08, C_concrete_d, C_concrete_d, 1.22f);
    b.beam(-0.05, -0.42, 1.30, -0.05, -0.42, 0.62, 0.012, C_soot, false);
}

/// A settlement: a paved footprint and a block field that steps with scale.
void form_settlement(builder& b, int scale, std::uint32_t seed, int cw, int r,
                     const double* avoid, int n_avoid, double s_px)
{
    scale = std::clamp(scale, 1, 5);
    static constexpr float k_radius[6] = { 0, 0.34f, 0.48f, 0.62f, 0.78f, 0.92f };
    static constexpr float k_cell[6]   = { 0, 0.20f, 0.19f, 0.18f, 0.17f, 0.16f };
    static constexpr float k_hmax[6]   = { 0, 0.07f, 0.11f, 0.17f, 0.30f, 0.44f };
    const float R = k_radius[scale];
    // At low bake resolution the grid coarsens so a block is never sub-pixel:
    // the far page then reads a city as a pale patch with dark massing.
    const float cell = std::max(k_cell[scale], static_cast<float>(2.6 / std::max(1.0, s_px)));
    const bool coarse = s_px < 12.0;
    if (coarse)
    {
        // The far page: a settlement is a pale paved patch with a dark core of
        // massing — how a city reads from orbit — sized up so a scale >= 3
        // centre is several pixels across where the old density dot stood.
        b.gell(0.0, 0.0, R * 1.25f, R * 1.10f, C_paved * 1.12f, std::min(0.95f, 0.50f + 0.12f * scale), R * 0.30f);
        b.gell(0.0, -0.04, R * 0.62f, R * 0.52f, C_slate * 0.80f, std::min(0.85f, 0.25f + 0.13f * scale), R * 0.25f);
    }
    else
        b.gell(0.0, 0.0, R * 1.10f, R * 0.98f, C_paved, std::min(0.92f, 0.55f + 0.09f * scale), R * 0.35f);

    const float off_x = (h01(cw, r, 0x5E70u) - 0.5f) * cell;
    const float off_y = (h01(cw, r, 0x5E71u) - 0.5f) * cell;
    const int   n = static_cast<int>(std::ceil(R / cell)) + 1;
    for (int j = -n; j <= n; ++j)
        for (int i = -n; i <= n; ++i)
        {
            // Streets: a spine each way through the larger settlements.
            if (scale >= 3 && (i == 0 || j == 1))
                continue;
            const double cx = i * cell + off_x, cy = j * cell + off_y;
            const double d  = std::sqrt(cx * cx + cy * cy) / R;
            if (d > 1.0)
                continue;
            bool blocked = false;
            for (int a = 0; a < n_avoid; ++a)
            {
                const double ax = cx - avoid[3 * a], ay = cy - avoid[3 * a + 1];
                if (ax * ax + ay * ay < avoid[3 * a + 2] * avoid[3 * a + 2])
                    blocked = true;
            }
            if (blocked)
                continue;
            const std::uint32_t salt = static_cast<std::uint32_t>((j + 64) * 131 + (i + 64));
            const float occ = h01(cw * 131 + i, r * 97 + j, 0x5E72u + salt);
            const float p_occ = (0.97f - 0.50f * static_cast<float>(d * d)) * (scale == 1 ? 0.75f : 1.0f);
            if (occ > p_occ)
                continue;
            const float jx = (h01(cw, r, 0x5E80u + salt) - 0.5f) * cell * 0.18f;
            const float jy = (h01(cw, r, 0x5E81u + salt) - 0.5f) * cell * 0.18f;
            const float fw = cell * (0.34f + 0.11f * h01(cw, r, 0x5E82u + salt));
            const float fd = cell * (0.31f + 0.10f * h01(cw, r, 0x5E83u + salt));
            const float central = static_cast<float>(1.0 - d);
            const float hr = h01(cw, r, 0x5E84u + salt);
            float h = k_hmax[scale] * (0.25f + 0.75f * central * central) * (0.55f + 0.45f * hr);
            h = std::max(h, 0.035f);
            // One roofscape per settlement — its dominant roof with a minority
            // of others — so a town reads as a place, not a heap of crates.
            const rgb tile_roof = mix(C_tile, C_slate, 0.18f);
            const rgb roofs[4] = { tile_roof, C_slate * 1.05f, tile_roof * 0.88f, C_concrete * 0.88f };
            const int dominant = (seed >> 1) % 2 + (scale >= 4 ? 1 : 0);
            const float pick = h01(cw, r, 0x5E85u + salt);
            const rgb roof = pick < 0.68f ? roofs[dominant] : roofs[static_cast<int>(pick * 13.0f) & 3];
            const rgb walls[3] = { C_plaster, C_plaster * 0.92f, C_stone_pale * 0.94f };
            const rgb wall = walls[static_cast<int>(occ * 30.0f) % 3];
            const double x = cx + jx, y = cy + jy;
            // b.k is 1 for settlements: local units are canonical.
            if (h < 0.12f)
                b.gable(x, y, fw, fd, h * 0.62f, h * 0.55f, roof, wall);
            else if (scale >= 4 && central > 0.55f && hr > 0.55f)
                b.box(x, y, fw * 0.85f, fd * 0.85f, h * 1.6f, C_concrete, C_glass * 0.95f);
            else
                b.box(x, y, fw, fd, h, roof * 1.05f, wall);
        }
    // A landmark at the heart of a town and above: a civic dome or a spire.
    if (scale >= 3 && n_avoid == 0)
    {
        const float lh = k_hmax[scale];
        if ((seed & 1) || scale >= 4)
            b.cyl(cell * 0.5, -cell * 0.6, cell * 0.42f, lh * 0.9f, top::dome, cell * 0.42f,
                  { 150, 162, 156 }, C_stone_pale);
        else
            b.cyl(cell * 0.5, -cell * 0.6, cell * 0.22f, lh * 1.8f, top::cone, lh * 0.8f,
                  C_slate, C_stone_pale);
    }
}

/// A razed centre: broken walls, rubble, scorched ground. Never roofed.
void form_ruin(builder& b, int cw, int r)
{
    b.gell(0.0, 0.0, 0.52f, 0.46f, C_char, 0.55f, 0.18f);
    for (int k = 0; k < 7; ++k)
    {
        const std::uint32_t s = 0xD400u + static_cast<std::uint32_t>(k) * 7u;
        const double cx = (h01(cw, r, s) - 0.5) * 0.80;
        const double cy = (h01(cw, r, s + 1) - 0.5) * 0.70;
        const float  w  = 0.06f + 0.06f * h01(cw, r, s + 2);
        const float  d  = 0.05f + 0.05f * h01(cw, r, s + 3);
        // A house outline with walls missing: back, sides, maybe the front.
        const float hb = 0.03f + 0.07f * h01(cw, r, s + 4);
        b.box(cx, cy - d, w, 0.012f, hb, C_ruin * 0.8f, C_ruin);
        if (h01(cw, r, s + 5) > 0.35f)
            b.box(cx - w, cy, 0.012f, d, hb * 0.7f, C_ruin * 0.8f, C_ruin);
        if (h01(cw, r, s + 6) > 0.55f)
            b.box(cx + w, cy, 0.012f, d, hb * 0.5f, C_ruin * 0.8f, C_ruin * 0.9f);
        b.heap(cx + w * 0.4, cy + d * 0.8, w * 0.7f, 0.025f, C_ruin * 0.75f);
    }
}

// --- Families --------------------------------------------------------------

extraction_family family_of_target(resource_type t)
{
    switch (t)
    {
        case resource_type::stone:
        case resource_type::sand:
        case resource_type::clay:
        case resource_type::silica:
        case resource_type::regolith:            return extraction_family::quarry;
        case resource_type::petroleum:           return extraction_family::oil_well;
        case resource_type::water:               return extraction_family::water_well;
        case resource_type::timber:              return extraction_family::timber;
        case resource_type::agricultural_produce:
        case resource_type::fibre:               return extraction_family::farm;
        case resource_type::tobacco:
        case resource_type::spices:
        case resource_type::coffee:              return extraction_family::plantation;
        case resource_type::peat:                return extraction_family::peat;
        case resource_type::hides:
        case resource_type::furs:                return extraction_family::trapping;
        default:                                 return extraction_family::mine;
    }
}

processing_family family_of_group(const std::string& g)
{
    struct row { const char* name; processing_family f; };
    static constexpr row rows[] = {
        { "Metal Foundry",          processing_family::metal_foundry },
        { "Fuel Production",        processing_family::fuel_production },
        { "Construction Materials", processing_family::construction_materials },
        { "Construction",           processing_family::construction },
        { "Artisan Goods",          processing_family::artisan_goods },
        { "Food Processing",        processing_family::food_processing },
        { "Chemical Works",         processing_family::chemical_works },
        { "Refinery",               processing_family::refinery },
        { "Power Generation",       processing_family::power_generation },
        { "Electronics",            processing_family::electronics },
        { "Advanced Fabrication",   processing_family::advanced_fabrication },
        { "Welfare Goods",          processing_family::welfare_goods },
    };
    for (const row& r : rows)
        if (g == r.name)
            return r.f;
    return processing_family::general;
}

/// Fixed in-tile layout: slot centres (canonical, from the tile centre) and
/// sizes, dominant first and front-most.
struct slot { double x, y; float size; };
constexpr slot k_slots[3][3] = {
    { {  0.00,  0.08, 0.84f }, { 0, 0, 0 },                { 0, 0, 0 } },
    { {  0.20,  0.26, 0.68f }, { -0.42, -0.30, 0.52f },    { 0, 0, 0 } },
    { {  0.02,  0.36, 0.62f }, { -0.47, -0.28, 0.48f },    { 0.47, -0.32, 0.48f } },
};

/// On a tile that also stands a settlement or ruin, the works take smaller
/// slots toward the tile's south and east edge so the town keeps its heart.
constexpr slot k_slots_settled[3][3] = {
    { {  0.42,  0.36, 0.50f }, { 0, 0, 0 },                { 0, 0, 0 } },
    { {  0.44,  0.38, 0.46f }, { -0.46,  0.40, 0.40f },    { 0, 0, 0 } },
    { {  0.44,  0.40, 0.42f }, { -0.46,  0.42, 0.38f },    { 0.02, -0.52, 0.36f } },
};

const slot& slot_for(int n, int j, bool settled)
{
    return settled ? k_slots_settled[n - 1][j] : k_slots[n - 1][j];
}

/// Vertical screen factor for a geometry (flat: k_flat_vertical; oblique:
/// stepping up to k_tilt_vertical at 45 degrees, then pre-stretched 1/cos).
double vertical_factor(const geometry& g)
{
    const double t = std::clamp((1.0 - g.tilt_sy) / (1.0 - 0.70710678), 0.0, 1.0);
    const double v_screen = k_flat_vertical + (k_tilt_vertical - k_flat_vertical) * t;
    return v_screen / g.tilt_sy;
}

/// The canonical reach of the pass around a window: tiles whose structures
/// (or shadows) can land in it. Shared by the stamp walk and the hash, so the
/// hash covers exactly what the pass reads.
struct reach { int r_lo, r_hi; double wx0, wx1; };
reach window_reach(const bake_source& src, const geometry& g, int px0, int py0, int pw, int ph)
{
    const double vz = vertical_factor(g);
    const double up = 1.1 + g.lift + k_max_height * vz;   // a structure SOUTH of the window rising into it
    const double dn = 1.1 + k_max_height * k_shadow_y;    // a structure NORTH whose base/shadow falls in
    const double wx0 = px0 / g.s - 1.2 - k_max_height * (k_shadow_x + k_shear * vz);
    const double wx1 = (px0 + pw) / g.s + 1.2;
    const double wy0 = py0 / g.s + g.y_min - dn;
    const double wy1 = (py0 + ph) / g.s + g.y_min + up;
    reach rr;
    rr.r_lo = std::max(0, static_cast<int>(std::floor(wy0 / 1.5)));
    rr.r_hi = std::min(src.gh - 1, static_cast<int>(std::ceil(wy1 / 1.5)));
    rr.wx0 = wx0; rr.wx1 = wx1;
    return rr;
}

/// The cardinal water direction from a port's tile (most-water axis), or
/// (0, 1) when it has none — a port inland of the snapshot still faces south.
void water_dir(const bake_source& src, int cw, int r, int& wx, int& wy)
{
    double sx = 0, sy = 0;
    const bool odd = (r & 1) != 0;
    // Pointy-top offset neighbours (odd rows shift right).
    const int dcs_even[6][2] = { {1,0}, {-1,0}, {0,-1}, {-1,-1}, {0,1}, {-1,1} };
    const int dcs_odd [6][2] = { {1,0}, {-1,0}, {1,-1}, {0,-1}, {1,1}, {0,1} };
    const auto& d = odd ? dcs_odd : dcs_even;
    for (int k = 0; k < 6; ++k)
    {
        const int nr = r + d[k][1];
        if (nr < 0 || nr >= src.gh)
            continue;
        const int nc = ((cw + d[k][0]) % src.gw + src.gw) % src.gw;
        const std::size_t j = static_cast<std::size_t>(nr) * src.gw + nc;
        if (src.cls[j] != static_cast<std::uint8_t>(bake_source::tile_class::water))
            continue;
        // Neighbour centre offset, canonical: its column plus its row's shift,
        // minus ours.
        sx += (d[k][0] + ((nr & 1) ? 0.5 : 0.0) - (odd ? 0.5 : 0.0)) * kSqrt3;
        sy += d[k][1] * 1.5;
    }
    if (std::fabs(sx) < 1e-9 && std::fabs(sy) < 1e-9)
    {
        wx = 0; wy = 1;
        return;
    }
    if (std::fabs(sx) >= std::fabs(sy)) { wx = sx > 0 ? 1 : -1; wy = 0; }
    else                                 { wx = 0; wy = sy > 0 ? 1 : -1; }
}

} // namespace

// =============================================================================
// Snapshot
// =============================================================================

void extract_installations(const world& w, entity_id body, const recipe_registry* reg,
                           bake_source& s)
{
    const std::size_t n = static_cast<std::size_t>(s.gw) * s.gh;
    s.inst.of_tile.assign(n, -1);
    s.inst.list.clear();
    if (n == 0)
        return;

    const auto raster_of = [&](entity_id tile) -> long long {
        const auto it = w.tiles.find(tile);
        if (it == w.tiles.end() || it->second.body != body)
            return -1;
        const tile_component& t = it->second;
        if (t.grid_x < 0 || t.grid_x >= s.gw || t.grid_y < 0 || t.grid_y >= s.gh)
            return -1;
        const std::size_t i = static_cast<std::size_t>(t.grid_y) * s.gw + t.grid_x;
        const auto c = static_cast<bake_source::tile_class>(s.cls[i]);
        // Survey mask: nothing on a masked tile is recorded, so a rival's works
        // cannot leak through the lock fill.
        if (c != bake_source::tile_class::land && c != bake_source::tile_class::water)
            return -1;
        return static_cast<long long>(i);
    };
    const auto slot_of = [&](std::size_t i) -> std::size_t {
        if (s.inst.of_tile[i] < 0)
        {
            s.inst.of_tile[i] = static_cast<std::int32_t>(s.inst.list.size());
            s.inst.list.emplace_back();
        }
        return static_cast<std::size_t>(s.inst.of_tile[i]);
    };

    // --- Buildings: grouped per tile into stacks ---------------------------
    struct rec { std::size_t tile; entity_id id; const building_component* b; };
    std::vector<rec> recs;
    recs.reserve(w.buildings.size());
    for (const auto& [bid, bc] : w.buildings)
    {
        const long long i = raster_of(bc.tile);
        if (i >= 0)
            recs.push_back({ static_cast<std::size_t>(i), bid, &bc });
    }
    // Ascending (tile, id): stacks then appear in order of their LOWEST member,
    // so stack 0 is the dominant one — placement_rules::stack_members' order.
    std::sort(recs.begin(), recs.end(), [](const rec& a, const rec& b) {
        return a.tile != b.tile ? a.tile < b.tile : a.id < b.id;
    });
    for (std::size_t a = 0; a < recs.size();)
    {
        std::size_t e = a;
        while (e < recs.size() && recs[e].tile == recs[a].tile)
            ++e;
        struct stk { building_type type; resource_type target; const building_component* rep; bool all_building; };
        std::vector<stk> stacks;
        for (std::size_t k = a; k < e; ++k)
        {
            const building_component& bc = *recs[k].b;
            // stack_members' grouping: (type, target), target only for extraction.
            const bool ext = bc.type == building_type::extraction_site;
            bool found = false;
            for (stk& st : stacks)
                if (st.type == bc.type && (!ext || st.target == bc.target_resource))
                {
                    st.all_building = st.all_building && bc.ticks_remaining > 0;
                    found = true;
                    break;
                }
            if (!found)
                stacks.push_back({ bc.type, bc.target_resource, &bc, bc.ticks_remaining > 0 });
        }
        tile_installation& ti = s.inst.list[slot_of(recs[a].tile)];
        ti.n_stacks = static_cast<std::uint8_t>(
            std::min<std::size_t>(stacks.size(), tile_installation::k_max_stacks));
        for (int j = 0; j < ti.n_stacks; ++j)
        {
            const stk& st = stacks[static_cast<std::size_t>(j)];
            stamp_key key;
            // A stack stands as scaffolding only while EVERY member is still a
            // site: once one is finished the structure stands.
            key.subject = st.all_building ? stamp_subject::scaffold : stamp_subject::building;
            key.type    = static_cast<std::uint8_t>(st.type);
            if (st.type == building_type::extraction_site)
                key.family = static_cast<std::uint8_t>(family_of_target(st.target));
            else if (st.type == building_type::processing_facility)
            {
                const recipe* rc = reg ? reg->get_recipe(st.rep->recipe) : nullptr;
                key.family = static_cast<std::uint8_t>(
                    rc ? family_of_group(rc->group) : processing_family::general);
            }
            ti.stacks[j] = key;
        }
        a = e;
    }

    // --- Population centres ------------------------------------------------
    std::vector<entity_id> centres;
    centres.reserve(w.population_centres.size());
    for (const auto& [pid, pc] : w.population_centres)
        centres.push_back(pid);
    std::sort(centres.begin(), centres.end()); // lowest id wins a shared tile
    for (const entity_id pid : centres)
    {
        const auto tit = w.population_centre_tile.find(pid);
        if (tit == w.population_centre_tile.end())
            continue;
        const long long i = raster_of(tit->second);
        if (i < 0)
            continue;
        tile_installation& ti = s.inst.list[slot_of(static_cast<std::size_t>(i))];
        if (ti.settlement.subject != stamp_subject::none)
            continue;
        const population_centre_component& pc = w.population_centres.at(pid);
        ti.settlement.subject = pc.razed ? stamp_subject::ruin : stamp_subject::settlement;
        ti.settlement.scale   = static_cast<std::uint8_t>(std::clamp(pc.scale, 1, 5));
    }
}

// =============================================================================
// The pass
// =============================================================================

void stamp_installations(const bake_source& src, const geometry& g, const bake_params& p,
                         int px0, int py0, int pw, int ph, std::uint32_t* out,
                         const std::uint8_t* tag)
{
    if (src.inst.list.empty() || src.inst.of_tile.size() != src.cls.size())
        return;

    raster R;
    R.out = out; R.tag = tag; R.pw = pw; R.ph = ph; R.px0 = px0; R.py0 = py0;
    R.W = std::max(1, g.W);

    view v;
    v.s = g.s; v.ss = g.ss; v.y_min = g.y_min; v.px0 = px0; v.py0 = py0;
    v.vz = vertical_factor(g);
    v.sh = k_shear * v.vz * g.tilt_sy; // the lean is a screen-space cue: unstretched

    const reach rr = window_reach(src, g, px0, py0, pw, ph);
    const bool fine = nominal_s(g) >= 30.0; // nominal: a 2x bake must not switch detail on (BL-1244)

    std::vector<part>     parts;
    std::vector<instance> inst;
    parts.reserve(512);

    for (int r = rr.r_lo; r <= rr.r_hi; ++r)
    {
        const double odd = (r & 1) ? 0.5 : 0.0;
        const int c_lo = static_cast<int>(std::floor(rr.wx0 / kSqrt3 - odd)) - 1;
        const int c_hi = static_cast<int>(std::ceil (rr.wx1 / kSqrt3 - odd)) + 1;
        for (int c = c_lo; c <= c_hi; ++c)
        {
            const int cw = ((c % src.gw) + src.gw) % src.gw;
            const std::size_t i = static_cast<std::size_t>(r) * src.gw + cw;
            const std::int32_t li = src.inst.of_tile[i];
            if (li < 0)
                continue;
            const tile_installation& ti = src.inst.list[static_cast<std::size_t>(li)];
            // Hashes on the WRAPPED coordinate, positions on the UNWRAPPED centre.
            const double hx = kSqrt3 * (c + odd);
            const double hy = 1.5 * r;
            const double lift = src.height[i] * g.lift;

            const auto begin_instance = [&](double sy, double sx, int tie) {
                instance in;
                in.sort_y = sy; in.sort_x = sx; in.tie = tie; in.lift = lift;
                in.first = parts.size();
                inst.push_back(in);
            };
            const auto end_instance = [&]() { inst.back().count = parts.size() - inst.back().first; };

            // The pad: cleared ground under the works, so a tile with something
            // on it reads as WORKED at distance (footprint contrast).
            if (ti.n_stacks > 0 && ti.settlement.subject == stamp_subject::none)
            {
                begin_instance(hy - 9.0, hx, 0); // pads sort first: ground
                builder pb{ parts, hx, hy, 1.0f, 1.0f, fine };
                const float pr = 0.66f + 0.06f * (ti.n_stacks - 1);
                pb.gell(0.0, 0.10, pr * 1.12f, pr * 0.92f, C_dirt, 0.58f, 0.26f);
                end_instance();
            }

            // Slots the stack structures take — a settlement's blocks keep off
            // them, and on a settled tile the works step aside to its edge.
            const bool settled = ti.settlement.subject != stamp_subject::none;
            double avoid[9];
            int    n_avoid = 0;
            const int ns = ti.n_stacks;
            for (int j = 0; j < ns; ++j)
            {
                const slot& sl = slot_for(ns, j, settled);
                avoid[3 * n_avoid]     = sl.x;
                avoid[3 * n_avoid + 1] = sl.y;
                avoid[3 * n_avoid + 2] = sl.size * 0.80;
                ++n_avoid;
            }

            if (ti.settlement.subject == stamp_subject::settlement)
            {
                // Each block is its own instance, so blocks interleave
                // back-to-front with the stack structures on the tile.
                std::vector<part> blocks;
                builder sb{ blocks, hx, hy, 1.0f, 1.0f, fine };
                form_settlement(sb, ti.settlement.scale,
                                static_cast<std::uint32_t>(h01(cw, r, 0x5E7Au) * 65535.0f),
                                cw, r, avoid, n_avoid, nominal_s(g));
                for (std::size_t q = 0; q < blocks.size(); ++q)
                {
                    const part& bp = blocks[q];
                    const bool ground = bp.k == pk::gell || bp.k == pk::grect;
                    begin_instance(ground ? hy - 8.0 : bp.y + bp.hd, bp.x, 10 + static_cast<int>(q));
                    parts.push_back(bp);
                    end_instance();
                }
            }
            else if (ti.settlement.subject == stamp_subject::ruin)
            {
                std::vector<part> bits;
                builder rb{ bits, hx, hy, 1.0f, 1.0f, fine };
                form_ruin(rb, cw, r);
                for (std::size_t q = 0; q < bits.size(); ++q)
                {
                    const part& bp = bits[q];
                    const bool ground = bp.k == pk::gell || bp.k == pk::grect;
                    begin_instance(ground ? hy - 8.0 : bp.y + bp.hd, bp.x, 10 + static_cast<int>(q));
                    parts.push_back(bp);
                    end_instance();
                }
            }

            for (int j = 0; j < ns; ++j)
            {
                const stamp_key& key = ti.stacks[j];
                const slot& sl = slot_for(ns, j, settled);
                const std::uint32_t seed = static_cast<std::uint32_t>(
                    h01(cw, r, 0xB1D0u + static_cast<std::uint32_t>(j)) * 16777216.0f);
                const double ax = hx + sl.x, ay = hy + sl.y;
                // Ground features (fields, pits, yards) of a form sort as
                // ground; the standing structure sorts at its slot's front.
                std::vector<part> fp;
                builder b{ fp, ax, ay, sl.size, (seed & 0x100u) ? -1.0f : 1.0f, fine };

                const authored_stamp* art = p.stamps ? p.stamps->find(key) : nullptr;
                if (art)
                {
                    // The authored half of the seam: a raster replaces the
                    // procedural form for this key. Drawn as a ground-anchored
                    // sprite in the same instance order.
                    // (No sheet ships yet; see stamp_sheet.)
                    begin_instance(ay + sl.size * 0.6, ax, 1 + j);
                    end_instance();
                    // Blit below, in the draw loop, keyed by this instance's tie.
                    inst.back().tie = -(1 + j) * 1000 - static_cast<int>(art - p.stamps->stamps.data());
                    continue;
                }

                if (key.subject == stamp_subject::scaffold)
                    form_scaffold(b, seed);
                else
                {
                    const auto bt = static_cast<building_type>(key.type);
                    if (bt == building_type::extraction_site)
                        form_extraction(b, static_cast<extraction_family>(key.family), seed);
                    else if (bt == building_type::processing_facility)
                        form_processing(b, static_cast<processing_family>(key.family), seed);
                    else if (bt == building_type::port)
                    {
                        int wx = 0, wy = 1;
                        water_dir(src, cw, r, wx, wy);
                        form_port(b, wx, wy);
                    }
                    else
                        form_other(b, bt, seed);
                }
                // Ground parts first (as one ground instance), standing parts after.
                begin_instance(hy - 7.0 + 0.01 * j, ax, 2 + j);
                for (const part& q : fp)
                    if (q.k == pk::gell || q.k == pk::grect)
                        parts.push_back(q);
                end_instance();
                begin_instance(ay + sl.size * 0.55, ax, 5 + j);
                for (const part& q : fp)
                    if (q.k != pk::gell && q.k != pk::grect)
                        parts.push_back(q);
                end_instance();
            }
        }
    }
    if (inst.empty())
        return;

    // Painter's order: ground first, then back (north) to front (south). The
    // sort key is the UNWRAPPED position, so a wrap copy orders identically.
    std::stable_sort(inst.begin(), inst.end(), [](const instance& a, const instance& b) {
        if (a.sort_y != b.sort_y) return a.sort_y < b.sort_y;
        if (a.sort_x != b.sort_x) return a.sort_x < b.sort_x;
        return a.tie < b.tie;
    });

    // 1. Ground parts (pads, fields, pits) — every instance that is ground.
    const auto is_ground = [&](const instance& in) {
        for (std::size_t q = in.first; q < in.first + in.count; ++q)
            if (parts[q].k != pk::gell && parts[q].k != pk::grect)
                return false;
        return in.count > 0;
    };
    for (const instance& in : inst)
        if (is_ground(in))
            for (std::size_t q = in.first; q < in.first + in.count; ++q)
                draw_part(R, v, in.lift, parts[q], mode::colour);

    // 2. Shadows: one union over every standing part, applied once — two
    // overlapping shadows never double-darken.
    R.shadow.assign(static_cast<std::size_t>(pw) * ph, 0.0f);
    for (const instance& in : inst)
        for (std::size_t q = in.first; q < in.first + in.count; ++q)
            draw_part(R, v, in.lift, parts[q], mode::shadow);
    for (std::size_t q = 0; q < R.shadow.size(); ++q)
    {
        const float sh = R.shadow[q];
        if (sh <= 0.0f || !tag[q])
            continue;
        const float m = 1.0f - k_shadow_dark * std::min(1.0f, sh);
        std::uint32_t& d = out[q];
        d = palette::col32(static_cast<int>(palette::col_r(d) * m * 0.97f),
                           static_cast<int>(palette::col_g(d) * m * 0.99f),
                           static_cast<int>(palette::col_b(d) * m), 255);
    }

    // 3. Standing structures, back to front.
    for (const instance& in : inst)
    {
        if (in.tie <= -1000)
        {
            // Authored stamp: bilinear-free nearest blit, alpha = coverage.
            const int idx = (-in.tie) % 1000;
            const authored_stamp& a = p.stamps->stamps[static_cast<std::size_t>(idx)];
            if (a.w <= 0 || a.h <= 0 || a.px.size() < static_cast<std::size_t>(a.w) * a.h)
                continue;
            // The instance's anchor is its sort position (slot front).
            const double wcan = a.width_canonical;
            const double hcan = wcan * a.h / a.w;
            const pt base = proj(v, in.lift, in.sort_x, in.sort_y, 0.0);
            const float sw = static_cast<float>(wcan * v.s);
            const float shp = static_cast<float>(hcan * v.s * v.vz);
            const float ox = base.x - a.anchor_x * sw, oy = base.y - a.anchor_y * shp;
            for (int y = std::max(0, static_cast<int>(oy)); y < std::min(ph, static_cast<int>(oy + shp) + 1); ++y)
                for (int x = std::max(0, static_cast<int>(ox)); x < std::min(pw, static_cast<int>(ox + sw) + 1); ++x)
                {
                    const int sx = std::clamp(static_cast<int>((x + 0.5f - ox) / sw * a.w), 0, a.w - 1);
                    const int sy = std::clamp(static_cast<int>((y + 0.5f - oy) / shp * a.h), 0, a.h - 1);
                    const std::uint32_t c = a.px[static_cast<std::size_t>(sy) * a.w + sx];
                    const float al = palette::col_a(c) / 255.0f;
                    if (al > 0.0f)
                        put(R, x, y, { static_cast<float>(palette::col_r(c)),
                                       static_cast<float>(palette::col_g(c)),
                                       static_cast<float>(palette::col_b(c)) }, al, false);
                }
            continue;
        }
        if (is_ground(in))
            continue;
        // Per part: its ink then its faces, so every member outlines itself
        // over whatever stands behind it.
        for (std::size_t q = in.first; q < in.first + in.count; ++q)
        {
            draw_part(R, v, in.lift, parts[q], mode::ink);
            draw_part(R, v, in.lift, parts[q], mode::colour);
        }
    }
}

std::uint64_t installation_hash(const bake_source& src, const geometry& g,
                                int px0, int py0, int pw, int ph)
{
    std::uint64_t h = 0x9E3779B97F4A7C15ull;
    auto mix = [&h](std::uint64_t v) { h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2); };
    if (src.inst.list.empty() || src.inst.of_tile.size() != src.cls.size())
        return h;
    const reach rr = window_reach(src, g, px0, py0, pw, ph);
    for (int r = rr.r_lo; r <= rr.r_hi; ++r)
    {
        const double odd = (r & 1) ? 0.5 : 0.0;
        const int c_lo = static_cast<int>(std::floor(rr.wx0 / kSqrt3 - odd)) - 1;
        const int c_hi = static_cast<int>(std::ceil (rr.wx1 / kSqrt3 - odd)) + 1;
        for (int c = c_lo; c <= c_hi; ++c)
        {
            const int cw = ((c % src.gw) + src.gw) % src.gw;
            const std::size_t i = static_cast<std::size_t>(r) * src.gw + cw;
            const std::int32_t li = src.inst.of_tile[i];
            if (li < 0)
                continue;
            const tile_installation& ti = src.inst.list[static_cast<std::size_t>(li)];
            mix(i);
            mix(ti.n_stacks);
            for (int j = 0; j < ti.n_stacks; ++j)
                mix(ti.stacks[j].packed());
            mix(ti.settlement.packed());
        }
    }
    return h;
}

float installation_clear_radius(const bake_source& src, std::size_t i)
{
    if (i >= src.inst.of_tile.size())
        return 0.0f;
    const std::int32_t li = src.inst.of_tile[i];
    if (li < 0)
        return 0.0f;
    const tile_installation& ti = src.inst.list[static_cast<std::size_t>(li)];
    float r = 0.0f;
    if (ti.n_stacks > 0)
        r = 0.72f + 0.04f * (ti.n_stacks - 1);
    if (ti.settlement.subject == stamp_subject::settlement)
        r = std::max(r, 0.22f + 0.15f * ti.settlement.scale);
    if (ti.settlement.subject == stamp_subject::ruin)
        r = std::max(r, 0.45f);
    return r;
}

const char* stamp_name(const stamp_key& k)
{
    switch (k.subject)
    {
        case stamp_subject::settlement: return "settlement";
        case stamp_subject::ruin:       return "ruin";
        case stamp_subject::scaffold:   return "scaffold";
        case stamp_subject::building:   return "building";
        default:                        return "none";
    }
}

} // namespace ui::ground
