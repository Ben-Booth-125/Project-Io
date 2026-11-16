// slice3d (fragment) - THE 3D GROUND (RENDERING.md § The 3D ground). The bake as the
// heightmesh's colour, lit by the sun with cast shadows from a shadow map; the close-zoom
// detail layer and the far rungs' flat map at draw time (BL-1667 (3D detail layer); the
// ground shader's passes, weighted by the drawn radius each pixel actually has, from its
// screen derivatives); the land lift; distant ground fading toward the sky; and the canvas's
// ground marks draped over all of it (BL-1668 (draped ground marks)). A building (vertex
// alpha 1) takes its own colour, lit and shadowed, and no detail.
//
// The bake already carries its own slope shading from its fixed light, so the mesh's light
// applies as a ratio against flat ground under that same light. SDL GPU's DXIL conventions:
// fragment samplers at t<n>/s<n> space2, fragment uniforms at b0 space3.

Texture2D    u_bake           : register(t0, space2);
SamplerState u_bake_sampler   : register(s0, space2);
Texture2D    u_shadow         : register(t1, space2);
SamplerState u_shadow_sampler : register(s1, space2);
Texture2D    u_tiles          : register(t2, space2);
SamplerState u_tiles_sampler  : register(s2, space2);
Texture2D    u_detail         : register(t3, space2);
SamplerState u_detail_sampler : register(s3, space2);
Texture2D    u_fine           : register(t4, space2);
SamplerState u_fine_sampler   : register(s4, space2);
Texture2D    u_rolling        : register(t5, space2);
SamplerState u_rolling_sampler : register(s5, space2);
Texture2D    u_map            : register(t6, space2);
SamplerState u_map_sampler    : register(s6, space2);
Texture2D    u_drape          : register(t7, space2);
SamplerState u_drape_sampler  : register(s7, space2);

cbuffer slice_fs : register(b0, space3)
{
    float4 u_light;  // xyz: toward the sun (world, normalised); w: the lift gamma (1 = off)
    float4 u_sky;    // rgb: the haze colour; w: where the haze starts (canonical units)
    float4 u_eye;    // xyz: the camera (camera-relative world); w: the bake's light's height
    float4 u_shade;  // x: shadow depth (0 off), y: depth bias, z: one shadow texel, w: 1 on the backdrop
    float4 u_org;    // xy: the canonical point at the world's origin; z: the clock (s); w: 1 / framebuffer scale
    float4 u_det;    // x: detail strength (0 off), y: coarse period, z: fine strength, w: fine period
    float4 u_detr;   // x, y: drawn radii (px) the detail fades in over; z, w: the flat map's (z 0 = off)
    float4 u_ink;    // x: coast ink half-width (px), y: river ink half-width (px), z: tile edge band, w: tile tone swing
    float4 u_bump;   // each family's coarse bump height: vegetation, forest, arid, rock
    float4 u_famk;   // each family's detail strength multiplier
    float4 u_wind;   // each family's share of the wind
    float4 u_anim;   // x: a mountain tile's bump multiplier, y: a gust's brightening, z: a blade's flutter, w: sand's creep
    float4 u_dmap;   // the drape's uv = canonical * xy + zw
    float4 u_misc;   // x: 1 with a drape, y: the fine layer's bump share, z: the detail's relief on the light, w: 1 with the tile table
};

struct PSIn
{
    float4 pos  : SV_Position;
    float2 uv   : TEXCOORD0;
    float3 nrm  : TEXCOORD1;
    float3 wpos : TEXCOORD2;
    float4 spos : TEXCOORD3;
    float4 col  : TEXCOORD4;
};

static const float k_s3 = 1.7320508075688772;
static const float k_sixth = 1.0471975511965976; // pi / 3
static const float3 k_coast_ink = float3(0.22, 0.18, 0.15);
static const float3 k_river_ink = float3(0.20, 0.36, 0.52);
// The wind's direction, canonical (y south): from the west-north-west.
static const float2 k_wind_dir = float2(0.94299, 0.33282);

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

// The tile table's texel: (land, family, mountain, height); zero off the grid.
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

// A tile's own variation: a hash of its wrapped cell - xy an offset into the textures
// (fractions of a period), z a tone.
float3 tile_hash(int2 cell, int w)
{
    const uint c = (uint)(((cell.x % w) + w) % w), r = (uint)max(cell.y, 0);
    uint hsh = (c * 73856093u) ^ (r * 19349663u) ^ 0x9E3779B9u;
    hsh ^= hsh >> 16; hsh *= 0x7FEB352Du; hsh ^= hsh >> 15; hsh *= 0x846CA68Bu; hsh ^= hsh >> 16;
    return float3((hsh & 1023u) / 1024.0, ((hsh >> 10) & 1023u) / 1024.0,
                  ((hsh >> 20) & 1023u) / 1024.0);
}

// The detail's bump height at canonical q for the two sides, blended by t; fk the fine
// layer's share.
float bump_at(float2 q, float2 s0, float2 s1, float2 o0, float2 o1, float t, float fk)
{
    const float4 c0 = u_detail.SampleLevel(u_detail_sampler, q / u_det.y + o0, 0);
    const float4 c1 = u_detail.SampleLevel(u_detail_sampler, q / u_det.y + o1, 0);
    const float a0 = family_pick(u_bump, s0.x) * lerp(1.0, u_anim.x, s0.y);
    const float a1 = family_pick(u_bump, s1.x) * lerp(1.0, u_anim.x, s1.y);
    float b0 = (family_pick(c0, s0.x) - 0.5) * a0;
    float b1 = (family_pick(c1, s1.x) - 0.5) * a1;
    if (fk > 0.0)
    {
        const float4 f0 = u_fine.SampleLevel(u_fine_sampler, q / u_det.w + o0, 0);
        const float4 f1 = u_fine.SampleLevel(u_fine_sampler, q / u_det.w + o1, 0);
        b0 += (family_pick(f0, s0.x) - 0.5) * a0 * u_misc.y * fk;
        b1 += (family_pick(f1, s1.x) - 0.5) * a1 * u_misc.y * fk;
    }
    return lerp(b0, b1, t);
}

// A gust at canonical point q: broad bands of the rolling texture drifting downwind, 0..1.
float gust_at(float2 q)
{
    const float2 g = (q - k_wind_dir * u_org.z * 0.6) / 6.0;
    return smoothstep(0.55, 0.8, u_rolling.SampleLevel(u_rolling_sampler, g, 0).r);
}

// A blade's flutter at canonical point q, -1..1: each blade-sized cell swings at its own
// phase and pace, so neighbouring blades are never in step.
float flutter_at(float2 q)
{
    const int2 c = int2(floor(q * 20.0));
    uint h = ((uint)c.x * 73856093u) ^ ((uint)c.y * 19349663u) ^ 0x68E31DA4u;
    h ^= h >> 16; h *= 0x7FEB352Du; h ^= h >> 15; h *= 0x846CA68Bu; h ^= h >> 16;
    const float ph = (h & 1023u) / 1024.0 * 6.2831853, pace = 4.0 + ((h >> 10) & 255u) / 255.0 * 3.0;
    return sin(u_org.z * pace + ph);
}

// 0 lit .. 1 shadowed: a 3x3 percentage-closer filter on the shadow map.
float shadow_at(float4 spos, float ndl)
{
    const float3 s = spos.xyz / spos.w;
    const float2 uv = float2(s.x * 0.5 + 0.5, 0.5 - s.y * 0.5);
    if (any(uv < 0.0) || any(uv > 1.0) || s.z > 1.0)
        return 0.0;
    const float bias = u_shade.y * (1.0 + 3.0 * (1.0 - saturate(ndl)));
    float sum = 0.0;
    [unroll] for (int y = -1; y <= 1; ++y)
        [unroll] for (int x = -1; x <= 1; ++x)
        {
            const float d = u_shadow.SampleLevel(u_shadow_sampler, uv + float2(x, y) * u_shade.z, 0).r;
            sum += (s.z - bias > d) ? 1.0 : 0.0;
        }
    return sum / 9.0;
}

float3 lift(float3 c, float gamma, float land)
{
    const float y = dot(c, float3(0.2126, 0.7152, 0.0722));
    float sc = pow(clamp(y, 1.0 / 255.0, 1.0), gamma - 1.0);
    const float mx = max(c.r, max(c.g, c.b));
    if (mx * sc > 1.0)
        sc = max(1.0, 1.0 / max(mx, 1.0 / 255.0));
    sc = 1.0 + (sc - 1.0) * land;
    return c * sc;
}

struct PSOut
{
    float4 colour : SV_Target0;
    float4 coord  : SV_Target1; // where this pixel lies in the drape: 16 bits an axis, 0 = none
};

PSOut main(PSIn i)
{
    const float2 p = i.wpos.xz + u_org.xy; // canonical (x east, y south)
    const float2 fwp = max(fwidth(p), float2(1e-6, 1e-6)); // canonical units per framebuffer px
    const float r_px = 1.0 / max(fwp.x, fwp.y) * u_org.w;  // the drawn hex radius here, px
    float3 n = normalize(i.nrm);
    const float3 L = u_light.xyz;
    float3 c;
    float ground_ratio = 1.0;
    if (i.col.a > 0.5)
    {
        // A building: its own colour, lit directly.
        c = i.col.rgb;
        const float ndl = dot(n, L);
        c *= max(ndl, 0.0) * 0.70 + 0.38;
        if (u_shade.x > 0.0 && u_shade.w < 0.5)
            c *= 1.0 - u_shade.x * shadow_at(i.spos, ndl);
    }
    else
    {
        float4 tex = u_bake.Sample(u_bake_sampler, i.uv);
        c = tex.rgb;
        float land_w = 1.0;
        float3 bn = n;
        if (u_misc.w > 0.5)
        {
            uint w, h;
            u_tiles.GetDimensions(w, h);
            const int wi = (int)w, hi = (int)h;
            const int2 cell = nearest_cell(p);
            const float4 own = tile_of(cell, wi, hi);
            const float2 v = p - centre_of(cell);
            const float ka = atan2(v.y, v.x) / k_sixth;
            const float k = round(ka);
            const float2 dir = float2(cos(k * k_sixth), sin(k * k_sixth));
            const float d = 0.5 * k_s3 - dot(v, dir);
            const int2 nb_cell = nearest_cell(centre_of(cell) + k_s3 * dir);
            const float4 nb = tile_of(nb_cell, wi, hi);
            float t = 0.0; // the neighbour's share
            if (u_ink.z > 0.0)
                t = 0.5 - 0.5 * smoothstep(0.0, u_ink.z, d);
            const float land = lerp(own.x, nb.x, t);
            land_w = land;
            // Detail stays off a pixel the bake marked (a road or an installation).
            uint cw_, ch_;
            u_bake.GetDimensions(cw_, ch_);
            const float2 tuv = float2(frac(i.uv.x), saturate(i.uv.y));
            const int2 texel = clamp(int2(tuv * float2(cw_, ch_)), int2(0, 0), int2(cw_ - 1, ch_ - 1));
            const bool marked = (((uint)round(u_bake.Load(int3(texel, 0)).b * 255.0)) & 1u) != 0u;
            const float wown = marked ? 0.0 : own.x, wnb = marked ? 0.0 : nb.x;
            const float dw = lerp(wown, wnb, t);
            // The detail's weight at this pixel's drawn radius (a glide or a tilt brings it in
            // smoothly), and each layer's share by its texels per pixel - full to 1.5, gone by 3.
            const float kr = saturate((r_px - u_detr.x) / max(u_detr.y - u_detr.x, 1e-3));
            const float strength = u_det.x * kr * kr * (3.0 - 2.0 * kr);
            uint dwid, dhei, fwid, fhei;
            u_detail.GetDimensions(dwid, dhei);
            u_fine.GetDimensions(fwid, fhei);
            const float fpx = max(fwp.x, fwp.y);
            const float coarse_share = saturate((3.0 - fpx / u_det.y * (float)dwid) / 1.5);
            const float fine_share = saturate((3.0 - fpx / u_det.w * (float)fwid) / 1.5);
            if (land > 0.0 && dw > 0.0 && strength > 0.0)
            {
                const float2 s_own = float2(own.y, own.z), s_nb = float2(nb.y, nb.z);
                const float3 v_own = tile_hash(cell, wi), v_nb = tile_hash(nb_cell, wi);
                const float2 o_own = v_own.xy, o_nb = v_nb.xy;
                // Sand creeps downwind; nothing else moves the lookup.
                const float2 q_own = p + (own.y > 1.5 && own.y < 2.5 ? k_wind_dir * u_org.z * u_anim.w : float2(0, 0));
                const float2 q_nb  = p + (nb.y > 1.5 && nb.y < 2.5 ? k_wind_dir * u_org.z * u_anim.w : float2(0, 0));
                const float k_own = family_pick(u_famk, own.y), k_nb = family_pick(u_famk, nb.y);
                const float4 dt0 = u_detail.SampleLevel(u_detail_sampler, q_own / u_det.y + o_own, 0);
                const float4 dt1 = u_detail.SampleLevel(u_detail_sampler, q_nb / u_det.y + o_nb, 0);
                float dv = lerp(wown * k_own * (family_pick(dt0, own.y) - 0.5),
                                wnb * k_nb * (family_pick(dt1, nb.y) - 0.5), t) * coarse_share;
                const float fk = (u_det.z / max(u_det.x, 1e-4)) * fine_share;
                if (fk > 0.0)
                {
                    const float4 ft0 = u_fine.SampleLevel(u_fine_sampler, q_own / u_det.w + o_own, 0);
                    const float4 ft1 = u_fine.SampleLevel(u_fine_sampler, q_nb / u_det.w + o_nb, 0);
                    dv += lerp(wown * k_own * (family_pick(ft0, own.y) - 0.5),
                               wnb * k_nb * (family_pick(ft1, nb.y) - 0.5), t) * fk;
                }
                dv *= strength;
                dv += lerp(wown * (v_own.z - 0.5), wnb * (v_nb.z - 0.5), t) * u_ink.w * (strength / max(u_det.x, 1e-4));
                // A gust lightens what it bends, and each blade flutters on its own inside it.
                if (u_anim.y > 0.0 || u_anim.z > 0.0)
                {
                    const float gust = gust_at(p);
                    const float tip0 = saturate((family_pick(dt0, own.y) - 0.5) * 4.0);
                    const float tip1 = saturate((family_pick(dt1, nb.y) - 0.5) * 4.0);
                    const float fl = flutter_at(p);
                    dv += lerp(wown * family_pick(u_wind, own.y) * (u_anim.y + u_anim.z * tip0 * fl),
                               wnb * family_pick(u_wind, nb.y) * (u_anim.y + u_anim.z * tip1 * fl), t)
                        * gust * (strength / max(u_det.x, 1e-4));
                }
                c = saturate(c * (1.0 + 2.0 * dv));
                // The detail's relief on the mesh's light: the bumps' slope, one coarse texel apart.
                if (u_misc.z > 0.0)
                {
                    const float eps = u_det.y / (float)dwid;
                    const float b0 = bump_at(q_own, s_own, s_nb, o_own, o_nb, t, fine_share);
                    const float bx = bump_at(q_own + float2(eps, 0), s_own, s_nb, o_own, o_nb, t, fine_share);
                    const float by = bump_at(q_own + float2(0, eps), s_own, s_nb, o_own, o_nb, t, fine_share);
                    const float2 bg = float2(bx - b0, by - b0) / eps * u_misc.z * dw * (strength / max(u_det.x, 1e-4));
                    bn = normalize(n + float3(-bg.x, 0.0, -bg.y));
                }
            }
            // The flat map, at the far rungs.
            if (u_detr.z > 0.0)
            {
                const float m = saturate((u_detr.z - r_px) / max(u_detr.z - u_detr.w, 1e-3));
                const float map_w = m * m * (3.0 - 2.0 * m);
                const float4 mo = map_of(cell, wi, hi);
                if (map_w > 0.0 && mo.a > 0.25)
                {
                    const float px_per_unit = 1.0 / max(fwp.x, fwp.y);
                    float3 col = mo.rgb;
                    const float4 mn = map_of(nb_cell, wi, hi);
                    if (mn.a > 0.25 && nb.x != own.x)
                        col = lerp(col, k_coast_ink, saturate(u_ink.x + 0.5 - d * px_per_unit));
                    // Rivers, on land: centre to the midpoint of each river side. Angle index
                    // a (0 E, 1 SE, 2 SW, 3 W, 4 NW, 5 NE, y south) is side (6 - a) % 6.
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
                        col = lerp(col, k_river_ink, saturate(u_ink.y + 0.5 - best * px_per_unit));
                    }
                    c = lerp(c, col, map_w);
                    land_w *= 1.0 - map_w;
                }
            }
        }
        // The mesh's light, against flat ground under the bake's own light.
        const float ndl  = dot(bn, L);
        const float lit_ = max(ndl, 0.0) * 0.85 + 0.15;
        const float flat_ = max(u_eye.w, 0.0) * 0.85 + 0.15;
        ground_ratio = clamp(lit_ / flat_, 0.25, 2.0);
        c *= ground_ratio;
        if (u_shade.x > 0.0 && u_shade.w < 0.5)
            c *= 1.0 - u_shade.x * shadow_at(i.spos, dot(n, L));
        if (u_light.w < 1.0)
            c = lift(c, u_light.w, u_misc.w > 0.5 ? land_w : 1.0);
    }
    const float dist = length(i.wpos - u_eye.xyz);
    c = lerp(c, u_sky.rgb, 0.5 * saturate((dist - u_sky.w) / (3.0 * u_sky.w)));
    // The canvas's ground marks are laid over this frame by the drape shader, in SDL's own
    // queue (drape.frag.hlsl); this pass says where each pixel's ground lies in the drape.
    PSOut o;
    o.colour = float4(saturate(c), 1.0);
    o.coord  = float4(0, 0, 0, 0);
    if (u_misc.x > 0.5)
    {
        const float2 duv = p * u_dmap.xy + u_dmap.zw;
        if (all(duv >= 0.0) && all(duv <= 1.0))
        {
            const uint2 q = (uint2)round(duv * 65534.0) + 1u;
            o.coord = float4(q.x >> 8, q.x & 255u, q.y >> 8, q.y & 255u) / 255.0;
        }
    }
    return o;
}
