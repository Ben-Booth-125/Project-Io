// border_band - the national border rule drawn per pixel (BL-1657 (SDF borders);
// PLANETARY.md § The national border band).
//
// The draw's own texture is an OWNER TABLE, one texel per tile in the odd-r offset
// grid: rgb = the tile's muted nation colour; alpha byte = bit 7 "this tile draws its
// rule" (owned and visible), bits 0-6 the owner KEY (0 unclaimed, 127 no tile, 1-126 a
// nation). The quad's UVs carry canonical position as lens_wash's do.
//
// A pixel in a drawing tile tests each of its six sides: where the neighbour has a
// different owner, the rule is the segment the vector pass draws - pulled inside the
// tile by the inset along the centre line, one hex side long - at the stroke's width in
// SCREEN pixels, so the band is the same weight at every zoom, and anti-aliased over
// one pixel. A side facing unclaimed ground takes the unclaimed scale on width and
// alpha. Each rule lies inside its own tile, so no pixel ever mixes two nations.
#include "common.hlsli"

cbuffer border_params : register(b0, space3)
{
    // x: screen px per canonical unit (hex_size * zoom); y: stroke px;
    // z: inset (canonical); w: the unclaimed scale.
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

float4 fetch(int2 cell, int w)
{
    const int c = ((cell.x % w) + w) % w;
    return u_texture.Load(int3(c, cell.y, 0));
}

PSOutput main(PSInput input)
{
    PSOutput output;
    uint w, h;
    u_texture.GetDimensions(w, h);
    const float2 p = float2(input.v_uv.x * k_s3 * w, input.v_uv.y * 1.5 * h);

    const int2 cell = nearest_cell(p);
    if (cell.y < 0 || cell.y >= (int)h)
        discard;
    const float4 own = fetch(cell, (int)w);
    const int a8 = (int)round(own.a * 255.0);
    const int key = a8 & 127;
    if ((a8 & 128) == 0 || key == 0 || key == 127)
        discard;

    const float hex_px = max(u_params.x, 1e-3);
    const float aa = 1.0 / hex_px;
    const float s0 = 0.5 * k_s3 - u_params.z;
    const float2 v = p - centre_of(cell);
    float best = 0.0;
    [unroll] for (int n = 0; n < 6; ++n)
    {
        const float2 dir = float2(cos(n * k_sixth), sin(n * k_sixth));
        const int2 nb = nearest_cell(centre_of(cell) + k_s3 * dir);
        if (nb.y < 0 || nb.y >= (int)h)
            continue;
        const float4 o = fetch(nb, (int)w);
        const int nk = ((int)round(o.a * 255.0)) & 127;
        if (nk == 127)
            continue; // no tile there: no neighbour, no rule
        if (nk == key && all(abs(o.rgb - own.rgb) < 0.002))
            continue; // the same owner: an interior side
        const bool political = nk != 0;
        const float scale = political ? 1.0 : u_params.w;
        const float half_w = 0.5 * max(1.0, u_params.y * scale) / hex_px;
        const float s = dot(v, dir);
        const float t = dot(v, float2(-dir.y, dir.x));
        const float across = saturate(0.5 + (half_w - abs(s - s0)) / aa);
        const float along  = saturate(0.5 + (0.5 - abs(t)) / aa);
        best = max(best, across * along * scale);
    }
    if (best <= 0.0)
        discard;
    output.o_color = float4(own.rgb, best) * input.v_color;
    return output;
}
