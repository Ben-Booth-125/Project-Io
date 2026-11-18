// water_glint - moving light on open water (BL-1660 (shader water); RENDERING.md
// § Ambient animation).
//
// The draw's own texture is a WATER TABLE, one texel per tile in the odd-r offset
// grid: r = 1 on a visible water tile, 0 elsewhere. The quad's UVs carry canonical
// position as lens_wash's do. Two scrolling height fields, lit by the low NW sun the
// bake is lit by, give a small specular glint that is drawn OVER the baked water as a
// translucent highlight - the bake stays the water's colour, the shader only adds light.
//
// Open water only: the glint fades to nothing within half a hex of any side that faces
// a non-water tile, so a coast, a shore shelf or a river mouth is never animated across
// the painted coastline; and it fades out below ~24 px a hex, where a wave would be
// smaller than a pixel and read as noise.
#include "common.hlsli"

cbuffer water_params : register(b0, space3)
{
    // x: seconds (the render-side clock); y: screen px per canonical unit;
    // z: strength (peak glint alpha); w: unused.
    float4 u_params;
};

static const float k_s3 = 1.7320508075688772;
static const float k_sixth = 1.0471975511965976;

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

float water_at(int2 cell, int w, int h)
{
    if (cell.y < 0 || cell.y >= h)
        return 0.0;
    const int c = ((cell.x % w) + w) % w;
    return u_texture.Load(int3(c, cell.y, 0)).r;
}

float hash21(float2 q)
{
    q = frac(q * float2(123.34, 456.21));
    q += dot(q, q + 45.32);
    return frac(q.x * q.y);
}

// Smooth value noise.
float vnoise(float2 q)
{
    const float2 i = floor(q);
    const float2 f = frac(q);
    const float2 u = f * f * (3.0 - 2.0 * f);
    const float a = hash21(i), b = hash21(i + float2(1, 0));
    const float c = hash21(i + float2(0, 1)), d = hash21(i + float2(1, 1));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float height(float2 p, float t)
{
    // Two fields, crossing, at about three and five waves a hex.
    return 0.6 * vnoise(p * 3.1 + float2(0.21, 0.07) * t)
         + 0.4 * vnoise(float2(p.y, -p.x) * 5.3 + float2(-0.13, 0.17) * t);
}

PSOutput main(PSInput input)
{
    PSOutput output;
    uint w, h;
    u_texture.GetDimensions(w, h);
    const float2 p = float2(input.v_uv.x * k_s3 * w, input.v_uv.y * 1.5 * h);

    const int2 cell = nearest_cell(p);
    if (water_at(cell, (int)w, (int)h) < 0.5)
        discard;

    // Distance to the nearest side facing a non-water tile.
    const float2 v = p - centre_of(cell);
    float shore = 1e3;
    [unroll] for (int n = 0; n < 6; ++n)
    {
        const float2 dir = float2(cos(n * k_sixth), sin(n * k_sixth));
        if (water_at(nearest_cell(centre_of(cell) + k_s3 * dir), (int)w, (int)h) < 0.5)
            shore = min(shore, 0.5 * k_s3 - dot(v, dir));
    }
    const float fade = smoothstep(0.45, 0.85, shore) * smoothstep(16.0, 40.0, u_params.y);
    if (fade <= 0.0)
        discard;

    const float t = u_params.x;
    const float e = 0.02;
    const float h0 = height(p, t);
    const float3 n = normalize(float3(-(height(p + float2(e, 0), t) - h0) / e,
                                      -(height(p + float2(0, e), t) - h0) / e, 6.0));
    const float3 l = normalize(float3(-1.0, -1.0, 1.1)); // the low NW sun (y is down)
    const float3 hv = normalize(l + float3(0, 0, 1));
    const float spec = pow(saturate(dot(n, hv)), 90.0);
    const float a = spec * fade * u_params.z;
    if (a <= 0.004)
        discard;
    output.o_color = float4(1.0, 0.99, 0.95, a) * input.v_color;
    return output;
}
