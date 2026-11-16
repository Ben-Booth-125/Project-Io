// ground - the ground images, with the land lift, the close-zoom detail and relief, and the
// far rungs' flat map applied at draw time (RENDERING.md § Art direction and palette, "The
// lift at draw time", BL-1659 (live grade shader); § Level of detail, "The two closest rungs
// add detail at draw time", BL-1658 (close-zoom detail layer), and "The farthest rung is a
// flat map", BL-1661 (far-zoom cartographic map)).
//
// The draw's own texture is a ground chunk, baked WITHOUT the lift and without detail.
//   - The TILE TABLE (slot 1, one texel per tile, read by Load): r = 1 on a land tile the
//     bake draws as land; g = its detail family (0..3, as g * 255); b = 1 on a mountain
//     tile (its bumps stand taller); a = its generation height (0..1), on every surveyed
//     tile, water included.
//   - The chunk's own STRUCTURE MARK: blue's lowest bit, read unfiltered, set where the
//     bake painted a road or an installation - no detail and no relief there.
//   - The ROLLING ground (slot 5; linear): one seamless height texture, broad swells.
//   - The DETAIL textures (slot 2 coarse, slot 3 fine; linear): four seamless greyscale
//     textures each, one per family, in r, g, b, a (vegetation, forest, arid, rock), mean
//     0.5. Read as a brightness ratio AND as a height (the relief's bumps). The sampler
//     clamps in v, so every lookup wraps its own coordinate.
//   - The MAP TABLE (slot 4, by Load): each surveyed tile's flat colour in rgb and, in a,
//     128 + its river sides (bit i = side i, 0 E, 1 NE, 2 NW, 3 W, 4 SW, 5 SE; a = 0 on a
//     tile the survey hides).
// Each pixel finds its tile from its SCREEN position through the canvas's inverse camera,
// so a tile boundary is a hex side, softened over a short band toward the neighbour across
// the nearest side. In order:
//   1. Detail: a luminance ratio of +-strength at each texture's extremes, coarse and fine
//      summed (the fine one fades out where its texels shrink below about two pixels).
//   2. Relief: the height field (the plane through the three nearest tile centres) plus the
//      detail's bumps, lit from the bake's light, then a short shadow march toward the light
//      through slope and bumps and a long one over the height field (§ "Depth at the fixed
//      angle", BL-1664 (fixed-angle depth)). The detail is read where the view ray first meets
//      the bumps (parallax), and grass, forest and sand move with the wind.
//   3. The lift (bake_region's apply_land_lift: a gamma on luminance, applied as a ratio of
//      the pixel's own channels, held where it would clip).
//   4. The flat map, mixed over all of it by the map weight: the tile's colour, inked where a
//      land and a water tile meet and along each river half-course (centre to side
//      midpoint), ink widths in screen pixels.
// Detail and relief are weighted by land, and are off on a marked pixel; the lift by land.
#include "common.hlsli"

Texture2D    u_tiles          : register(t1, space2);
SamplerState u_tiles_sampler  : register(s1, space2);
Texture2D    u_detail         : register(t2, space2);
SamplerState u_detail_sampler : register(s2, space2);
Texture2D    u_fine           : register(t3, space2);
SamplerState u_fine_sampler   : register(s3, space2);
Texture2D    u_map            : register(t4, space2);
SamplerState u_map_sampler    : register(s4, space2);
Texture2D    u_rolling        : register(t5, space2);
SamplerState u_rolling_sampler : register(s5, space2);

cbuffer ground_params : register(b0, space3)
{
    float4 u_cam;    // canonical x = cam.x * px + cam.y, canonical y = cam.z * py + cam.w
    float4 u_lift;   // x: lift gamma (1 = off), y: edge band (canonical units),
                     // z: detail strength (0 = off), w: detail period (canonical units)
    float4 u_finep;  // x: fine detail strength (0 = off), y: fine period (canonical units)
    float4 u_mapp;   // x: flat-map weight (0 = off), y: coast ink half-width px, z: river ink half-width px
    float4 u_bump;   // coarse bump height per family (canonical units, full swing): vegetation, forest, arid, rock
    float4 u_relief; // x: relief weight (0 = off), y: height scale (canonical units per unit of
                     // tile height), z: shadow depth (0..1), w: the fine layer's bump share
    float4 u_relief2; // x: a mountain tile's bump multiplier, y: rolling ground's full swing
                      // (canonical units), z: its repeat (canonical units)
    float4 u_vary;    // x: per-tile texture offset (0 off, 1 on), y: per-tile tone swing,
                      // z: the height map's own shading share (0..1)
    float4 u_famk;    // each family's detail strength multiplier: vegetation, forest, arid, rock
    float4 u_depth;   // x: parallax depth (multiple of the bump heights, 0 = off), y: the view's
                      // tilt (canonical rise on screen per unit of height), z: the long shadow's
                      // reach (canonical units, 0 = off), w: its depth (0..1)
    float4 u_anim;    // x: the animation clock (seconds), y: a gust's brightening, z: a blade's
                      // flutter (brightness swing), w: sand's creep (canonical units per second)
    float4 u_wind;    // each family's share of the wind: vegetation, forest, arid, rock
};

// SDL's vertex shader emits the screen position after the colour and the UV.
struct PSGroundInput {
    float4 v_color : TEXCOORD0;
    float2 v_uv    : TEXCOORD1;
    float4 v_pos   : SV_Position;
};

static const float k_s3 = 1.7320508075688772;
static const float k_sixth = 1.0471975511965976; // pi / 3

// The bake's light: from the upper left (screen y down), about 40 degrees up.
static const float2 k_light_dir = float2(-0.70710678, -0.70710678); // toward the light, flat
static const float3 k_light     = float3(-0.54167, -0.54167, 0.64279);
static const float  k_light_tan = 0.83910; // rise per unit run toward the light

static const float3 k_coast_ink = float3(0.22, 0.18, 0.15);
static const float3 k_river_ink = float3(0.20, 0.36, 0.52);

float2 centre_of(int2 cell)
{
    return float2(k_s3 * (cell.x + 0.5 * (cell.y & 1)), 1.5 * cell.y);
}

int2 nearest_cell(float2 p)
{
    const int r0 = (int)floor(p.y / 1.5);
    int2 best = int2(0, r0);
    float bd = 1e30;
    [unroll] for (int k = 0; k < 2; ++k)
    {
        const int r = r0 + k;
        const int2 cell = int2((int)round(p.x / k_s3 - 0.5 * (r & 1)), r);
        const float2 d = p - centre_of(cell);
        const float dd = dot(d, d);
        if (dd < bd) { bd = dd; best = cell; }
    }
    return best;
}

// The tile table's texel: (land, family, structure, height); zero off the grid.
float4 tile_of(int2 cell, int w, int h)
{
    if (cell.y < 0 || cell.y >= h)
        return float4(0, 0, 0, 0);
    const int c = ((cell.x % w) + w) % w;
    const float4 t = u_tiles.Load(int3(c, cell.y, 0));
    return float4(t.r, round(t.g * 255.0), t.b, t.a);
}

float4 map_of(int2 cell, int w, int h) // the map table's texel; a = 0 off the grid
{
    if (cell.y < 0 || cell.y >= h)
        return float4(0, 0, 0, 0);
    const int c = ((cell.x % w) + w) % w;
    return u_map.Load(int3(c, cell.y, 0));
}

float family_pick(float4 d, float fam)
{
    return fam < 0.5 ? d.r : fam < 1.5 ? d.g : fam < 2.5 ? d.b : d.a;
}

// A tile's own variation (Ben, 2026-11-16: tiles should read on their own): a hash of
// its wrapped cell - xy an offset into the textures (fractions of a period), z a tone.
float3 tile_hash(int2 cell, int w)
{
    const uint c = (uint)(((cell.x % w) + w) % w), r = (uint)max(cell.y, 0);
    uint hsh = (c * 73856093u) ^ (r * 19349663u) ^ 0x9E3779B9u;
    hsh ^= hsh >> 16; hsh *= 0x7FEB352Du; hsh ^= hsh >> 15; hsh *= 0x846CA68Bu; hsh ^= hsh >> 16;
    return float3((hsh & 1023u) / 1024.0, ((hsh >> 10) & 1023u) / 1024.0,
                  ((hsh >> 20) & 1023u) / 1024.0);
}

// The bump height at canonical point q for the two sides (family, mountain flag), each
// read at its own tile's offset (o0, o1: fractions of a period), blended by t. fk: the
// fine layer's share (0 where it would shimmer).
float bump_at(float2 q0, float2 q1, float2 s0, float2 s1, float2 o0, float2 o1, float t, float fk)
{
    const float4 c0 = u_detail.SampleLevel(u_detail_sampler, frac(q0 / u_lift.w + o0), 0);
    const float4 c1 = u_detail.SampleLevel(u_detail_sampler, frac(q1 / u_lift.w + o1), 0);
    const float a0 = family_pick(u_bump, s0.x) * lerp(1.0, u_relief2.x, s0.y);
    const float a1 = family_pick(u_bump, s1.x) * lerp(1.0, u_relief2.x, s1.y);
    float b0 = (family_pick(c0, s0.x) - 0.5) * a0;
    float b1 = (family_pick(c1, s1.x) - 0.5) * a1;
    if (fk > 0.0)
    {
        const float4 f0 = u_fine.SampleLevel(u_fine_sampler, frac(q0 / u_finep.y + o0), 0);
        const float4 f1 = u_fine.SampleLevel(u_fine_sampler, frac(q1 / u_finep.y + o1), 0);
        b0 += (family_pick(f0, s0.x) - 0.5) * a0 * u_relief.w * fk;
        b1 += (family_pick(f1, s1.x) - 0.5) * a1 * u_relief.w * fk;
    }
    return lerp(b0, b1, t);
}

// The rolling ground's height at canonical point q.
float rolling_at(float2 q)
{
    return (u_rolling.SampleLevel(u_rolling_sampler, frac(q / u_relief2.z), 0).r - 0.5) * u_relief2.y;
}

float lit(float3 n)
{
    return max(dot(normalize(n), k_light), 0.05);
}

// The wind's direction, flat (screen y down): from the west-north-west.
static const float2 k_wind_dir = float2(0.94299, 0.33282);

// The height field at any canonical point: the plane through the three nearest tile
// centres (as the relief computes it for its own pixel), scaled, plus the rolling ground.
float field_h(float2 q, int wi, int hi)
{
    const int2 c0 = nearest_cell(q);
    const float2 P0 = centre_of(c0);
    const float2 v = q - P0;
    const float ka = atan2(v.y, v.x) / k_sixth;
    const float k = round(ka);
    const float k2 = (ka - k) >= 0.0 ? k + 1.0 : k - 1.0;
    const int2 c1 = nearest_cell(P0 + k_s3 * float2(cos(k * k_sixth), sin(k * k_sixth)));
    const int2 c2 = nearest_cell(P0 + k_s3 * float2(cos(k2 * k_sixth), sin(k2 * k_sixth)));
    const float hs = u_relief.y;
    const float h0 = tile_of(c0, wi, hi).w * hs, h1 = tile_of(c1, wi, hi).w * hs,
                h2 = tile_of(c2, wi, hi).w * hs;
    const float2 e1 = centre_of(c1) - P0, e2 = centre_of(c2) - P0;
    const float det = e1.x * e2.y - e1.y * e2.x;
    float h = h0;
    if (abs(det) > 1e-4)
        h += dot(v, float2((h1 - h0) * e2.y - (h2 - h0) * e1.y,
                           (h2 - h0) * e1.x - (h1 - h0) * e2.x) / det);
    return h + rolling_at(q);
}

// A gust at canonical point q: broad bands of the rolling texture drifting downwind, 0..1.
float gust_at(float2 q)
{
    const float2 g = (q - k_wind_dir * u_anim.x * 0.6) / 6.0;
    return smoothstep(0.55, 0.8, u_rolling.SampleLevel(u_rolling_sampler, frac(g), 0).r);
}

// The wind's offset of one side's detail (family fam): sand creeps downwind. Nothing else
// moves the lookup - a shifted lookup warps the whole ground (Ben, 2026-11-16: "looks more
// like the ground is warping than individual blades of grass are waving").
float2 wind_offset(float2 q, float fam, float gust)
{
    const float creep = fam > 1.5 && fam < 2.5 ? u_anim.x * u_anim.w : 0.0;
    return k_wind_dir * creep;
}

// A blade's flutter at canonical point q, -1..1: each blade-sized cell (twenty to a unit)
// swings at its own phase and pace, so neighbouring blades are never in step.
float flutter_at(float2 q)
{
    const int2 c = int2(floor(q * 20.0));
    uint h = ((uint)c.x * 73856093u) ^ ((uint)c.y * 19349663u) ^ 0x68E31DA4u;
    h ^= h >> 16; h *= 0x7FEB352Du; h ^= h >> 15; h *= 0x846CA68Bu; h ^= h >> 16;
    const float ph = (h & 1023u) / 1024.0 * 6.2831853, pace = 4.0 + ((h >> 10) & 255u) / 255.0 * 3.0;
    return sin(u_anim.x * pace + ph);
}

PSOutput main(PSGroundInput input)
{
    PSOutput output;
    float4 tex = u_texture.Sample(u_sampler, input.v_uv);
    const float gamma = u_lift.x, strength = u_lift.z;
    const float map_w = u_mapp.x, relief_w = u_relief.x;
    // Canonical units per pixel, from the camera (no derivatives needed: it is affine).
    const float2 fp = abs(float2(u_cam.x, u_cam.z));
    if (gamma < 1.0 || strength > 0.0 || map_w > 0.0 || relief_w > 0.0)
    {
        uint w, h;
        u_tiles.GetDimensions(w, h);
        const int wi = (int)w, hi = (int)h;
        const float2 p = float2(u_cam.x * input.v_pos.x + u_cam.y, u_cam.z * input.v_pos.y + u_cam.w);
        const int2 cell = nearest_cell(p);
        const float4 own = tile_of(cell, wi, hi);
        // The nearest hex side: its direction, the neighbour across it, the distance to it.
        const float2 v = p - centre_of(cell);
        const float ka = atan2(v.y, v.x) / k_sixth;
        const float k = round(ka);
        const float2 dir = float2(cos(k * k_sixth), sin(k * k_sixth));
        const float d = 0.5 * k_s3 - dot(v, dir);
        const int2 nb_cell = nearest_cell(centre_of(cell) + k_s3 * dir);
        const float4 nb = tile_of(nb_cell, wi, hi);
        float t = 0.0; // the neighbour's share
        const float band = u_lift.y;
        if (band > 0.0)
            t = 0.5 - 0.5 * smoothstep(0.0, band, d);
        const float land = lerp(own.x, nb.x, t);
        // Detail and relief stay off a pixel the bake marked (a road or an installation).
        uint cw_, ch_;
        u_texture.GetDimensions(cw_, ch_);
        const int2 texel = clamp(int2(input.v_uv * float2(cw_, ch_)), int2(0, 0), int2(cw_ - 1, ch_ - 1));
        const bool marked = (((uint)round(u_texture.Load(int3(texel, 0)).b * 255.0)) & 1u) != 0u;
        const float wown = marked ? 0.0 : own.x, wnb = marked ? 0.0 : nb.x;
        const float dw = lerp(wown, wnb, t);
        const float2 s_own = float2(own.y, own.z), s_nb = float2(nb.y, nb.z);
        const float3 v_own = tile_hash(cell, wi), v_nb = tile_hash(nb_cell, wi);
        const float2 o_own = v_own.xy * u_vary.x, o_nb = v_nb.xy * u_vary.x;
        // Parallax: where the view ray first meets the bumps, stepped down through them along
        // the view's tilt (a raised point shows nearer the screen's top). Coarse bumps only.
        float2 pp = p;
        if (land > 0.0 && dw > 0.0 && relief_w > 0.0 && u_depth.x > 0.0)
        {
            const float am = 0.5 * u_depth.x * max(
                family_pick(u_bump, own.y) * lerp(1.0, u_relief2.x, own.z),
                family_pick(u_bump, nb.y) * lerp(1.0, u_relief2.x, nb.z));
            float h_prev = am, b_prev = am;
            bool hit = false;
            [unroll] for (int i = 0; i <= 8; ++i)
            {
                if (hit)
                    continue;
                const float h_ray = am - 2.0 * am * (float)i / 8.0;
                const float2 q = p + float2(0.0, u_depth.y * h_ray);
                const float hb = u_depth.x * bump_at(q, q, s_own, s_nb, o_own, o_nb, t, 0.0);
                if (hb >= h_ray)
                {
                    // Between the last step above the surface and this one below it.
                    const float a = i == 0 ? 0.0 : saturate((h_prev - b_prev) / max((h_prev - b_prev) - (h_ray - hb), 1e-5));
                    const float h_hit = lerp(h_prev, h_ray, a);
                    pp = p + float2(0.0, u_depth.y * h_hit);
                    hit = true;
                }
                h_prev = h_ray; b_prev = hb;
            }
            if (!hit)
                pp = p + float2(0.0, -u_depth.y * am);
            pp = lerp(p, pp, relief_w * dw);
        }
        // The wind: one gust field, each side's own sway and creep.
        const float gust = u_anim.z > 0.0 || u_anim.y > 0.0 ? gust_at(pp) : 0.0;
        const float2 q_own = pp + wind_offset(pp, own.y, gust), q_nb = pp + wind_offset(pp, nb.y, gust);
        // The fine layer's share: texels per pixel, full to 1.5, gone by 3.
        float fine_share = 0.0;
        uint fw = 1, fh = 1;
        if (u_finep.x > 0.0)
        {
            u_fine.GetDimensions(fw, fh);
            const float tpp = max(fp.x, fp.y) / u_finep.y * (float)fw;
            fine_share = saturate((3.0 - tpp) / 1.5);
        }
        if (land > 0.0)
        {
            if (strength > 0.0 && dw > 0.0)
            {
                // Each side reads at its own tile's offset, at its family's strength, with
                // its own tone, so a tile does not run on into its neighbour as wallpaper.
                const float k_own = family_pick(u_famk, own.y), k_nb = family_pick(u_famk, nb.y);
                const float4 dt0 = u_detail.SampleLevel(u_detail_sampler, frac(q_own / u_lift.w + o_own), 0);
                const float4 dt1 = u_detail.SampleLevel(u_detail_sampler, frac(q_nb / u_lift.w + o_nb), 0);
                float dv = lerp(wown * k_own * (family_pick(dt0, own.y) - 0.5),
                                wnb * k_nb * (family_pick(dt1, nb.y) - 0.5), t) * strength;
                const float fk = u_finep.x * fine_share;
                if (fk > 0.0)
                {
                    const float4 ft0 = u_fine.SampleLevel(u_fine_sampler, frac(q_own / u_finep.y + o_own), 0);
                    const float4 ft1 = u_fine.SampleLevel(u_fine_sampler, frac(q_nb / u_finep.y + o_nb), 0);
                    dv += lerp(wown * k_own * (family_pick(ft0, own.y) - 0.5),
                               wnb * k_nb * (family_pick(ft1, nb.y) - 0.5), t) * fk;
                }
                dv += lerp(wown * (v_own.z - 0.5), wnb * (v_nb.z - 0.5), t) * u_vary.y * relief_w;
                // A gust lightens what it bends (the blades' paler sides), and each blade
                // (the texture's raised, bright parts) flutters on its own inside it.
                const float tip0 = saturate((family_pick(dt0, own.y) - 0.5) * 4.0);
                const float tip1 = saturate((family_pick(dt1, nb.y) - 0.5) * 4.0);
                const float fl = flutter_at(pp);
                dv += lerp(wown * family_pick(u_wind, own.y) * (u_anim.y + u_anim.z * tip0 * fl),
                           wnb * family_pick(u_wind, nb.y) * (u_anim.y + u_anim.z * tip1 * fl), t)
                    * gust * relief_w;
                tex.rgb = saturate(tex.rgb * (1.0 + 2.0 * dv));
            }
            if (relief_w > 0.0 && dw > 0.0)
            {
                // The height field: the plane through this tile's centre and the two
                // neighbours' on the pixel's side of the nearest side's direction.
                const float k2 = (ka - k) >= 0.0 ? k + 1.0 : k - 1.0;
                const float2 dir2 = float2(cos(k2 * k_sixth), sin(k2 * k_sixth));
                const int2 c2 = nearest_cell(centre_of(cell) + k_s3 * dir2);
                const float hs = u_relief.y;
                const float2 P0 = centre_of(cell), P1 = centre_of(nb_cell), P2 = centre_of(c2);
                const float h0 = own.w * hs, h1 = nb.w * hs, h2 = tile_of(c2, wi, hi).w * hs;
                const float2 e1 = P1 - P0, e2 = P2 - P0;
                const float det = e1.x * e2.y - e1.y * e2.x;
                float2 grad = float2(0, 0);
                if (abs(det) > 1e-4)
                    grad = float2((h1 - h0) * e2.y - (h2 - h0) * e1.y,
                                  (h2 - h0) * e1.x - (h1 - h0) * e2.x) / det;
                // The bumps' slope by forward differences, one coarse texel apart.
                const float fk = fine_share;
                const float eps = u_lift.w / 256.0;
                const float2 ex = float2(eps, 0), ey = float2(0, eps);
                const float b  = bump_at(q_own, q_nb, s_own, s_nb, o_own, o_nb, t, fk);
                const float bx = bump_at(q_own + ex, q_nb + ex, s_own, s_nb, o_own, o_nb, t, fk);
                const float by = bump_at(q_own + ey, q_nb + ey, s_own, s_nb, o_own, o_nb, t, fk);
                float2 bgrad = float2(bx - b, by - b) / eps;
                // The rolling ground: broad swells the bake does not shade, so they count
                // with the bumps (what the shader adds), not with the bare slope.
                const float reps = u_relief2.z / 256.0;
                const float r0 = rolling_at(p);
                bgrad += float2(rolling_at(p + float2(reps, 0)) - r0, rolling_at(p + float2(0, reps)) - r0) / reps;
                const float flat_ = lit(float3(0.0, 0.0, 1.0));
                const float bare  = lit(float3(-grad, 1.0));
                const float bumpy = lit(float3(-(grad + bgrad), 1.0));
                // What the bumps and swells add over the bare slope, and (Ben, 2026-11-16:
                // the height map must read) the height map's own slope over flat ground, at
                // its share - the bake's shading of the slope is soft, so this restates it.
                float ratio = clamp(bumpy / bare * pow(bare / flat_, u_vary.z), 0.35, 1.9);
                // Short shadows: four steps toward the light, half a hex radius out.
                const float h_here = b + r0; // the plane's own rise cancels: compare against it
                float shade = 0.0;
                [unroll] for (int s = 1; s <= 4; ++s)
                {
                    const float run = 0.5 * (float)s / 4.0;
                    const float2 dq = k_light_dir * run;
                    const float rise = dot(grad, k_light_dir) * run
                                     + bump_at(q_own + dq, q_nb + dq, s_own, s_nb, o_own, o_nb, t, 0.0)
                                     + rolling_at(p + dq) - h_here;
                    shade = max(shade, saturate((rise - k_light_tan * run) / 0.02));
                }
                ratio *= 1.0 - u_relief.z * shade;
                // Long shadows: six steps over the height field, out to the reach - a high
                // tile shades the low ground beside it. Softer and lighter than the bumps'.
                if (u_depth.z > 0.0)
                {
                    const float f0 = field_h(p, wi, hi);
                    float lshade = 0.0;
                    [unroll] for (int j = 1; j <= 6; ++j)
                    {
                        const float run = u_depth.z * (float)j / 6.0;
                        const float rise = field_h(p + k_light_dir * run, wi, hi) - f0;
                        lshade = max(lshade, saturate((rise - k_light_tan * run) / 0.3));
                    }
                    ratio *= 1.0 - u_depth.w * lshade;
                }
                ratio = 1.0 + (ratio - 1.0) * relief_w * dw;
                tex.rgb = saturate(tex.rgb * ratio);
            }
            if (gamma < 1.0)
            {
                const float y = dot(tex.rgb, float3(0.2126, 0.7152, 0.0722));
                float sc = pow(clamp(y, 1.0 / 255.0, 1.0), gamma - 1.0);
                const float mx = max(tex.r, max(tex.g, tex.b));
                if (mx * sc > 1.0)
                    sc = max(1.0, 1.0 / max(mx, 1.0 / 255.0));
                sc = 1.0 + (sc - 1.0) * land;
                tex.rgb = saturate(tex.rgb * sc);
            }
        }
        if (map_w > 0.0)
        {
            const float4 mo = map_of(cell, wi, hi);
            if (mo.a > 0.25) // surveyed
            {
                const float px_per_unit = 1.0 / max(fp.x, fp.y);
                float3 col = mo.rgb;
                // The coast: this tile and the one across the nearest side are both
                // surveyed, and one is land and the other water. Each inks its own half.
                const float4 mn = map_of(nb_cell, wi, hi);
                if (mn.a > 0.25 && nb.x != own.x)
                {
                    const float ink = saturate(u_mapp.y + 0.5 - d * px_per_unit);
                    col = lerp(col, k_coast_ink, ink);
                }
                // Rivers, on land: centre to the midpoint of each river side. Screen angle
                // index a (0 E, 1 SE, 2 SW, 3 W, 4 NW, 5 NE, y down) is side (6 - a) % 6.
                const uint bits = ((uint)round(mo.a * 255.0)) & 63u;
                if (own.x > 0.5 && bits != 0u)
                {
                    float best = 1e30;
                    [unroll] for (int a = 0; a < 6; ++a)
                    {
                        const uint side = (uint)((6 - a) % 6);
                        if ((bits & (1u << side)) == 0u)
                            continue;
                        const float2 e = 0.5 * k_s3 * float2(cos(a * k_sixth), sin(a * k_sixth));
                        const float h01 = saturate(dot(v, e) / dot(e, e));
                        best = min(best, length(v - e * h01));
                    }
                    const float ink = saturate(u_mapp.z + 0.5 - best * px_per_unit);
                    col = lerp(col, k_river_ink, ink);
                }
                tex.rgb = lerp(tex.rgb, col, map_w);
            }
        }
    }
    output.o_color = tex * input.v_color;
    return output;
}
