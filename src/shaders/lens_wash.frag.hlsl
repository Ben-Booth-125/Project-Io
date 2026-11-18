// lens_wash - a lens's wash from a per-tile colour table (BL-1656 (lens lookup);
// LENSES.md § A lens washes the rendered ground).
//
// The draw's own texture is the TABLE: one texel per tile, (col, row) in the odd-r
// offset grid, holding the wash colour with its strength in alpha (0 = no wash).
// The quad's UVs carry canonical position: u * sqrt(3) * gw, v * 1.5 * gh, where a
// tile's centre is (sqrt(3) * (c + 0.5 * (r & 1)), 1.5 * r) - hex_local_centre at
// hex_size 1. Columns wrap (the cylinder); rows outside the grid draw nothing.
//
// Each pixel finds its tile (the nearest centre) and the neighbour across its
// nearest side, and within the edge band blends toward that neighbour, reaching
// half and half on the side itself - so two washes meet in a soft seam and a
// wash fades out over a band at its rim, never a hex staircase. The blend is in
// premultiplied alpha, so an unwashed neighbour fades the strength, not the hue.
#include "common.hlsli"

cbuffer lens_params : register(b0, space3)
{
    float4 u_params; // x: edge band, canonical units (0 = hard hex edges)
};

static const float k_s3 = 1.7320508075688772;

float2 centre_of(int2 cell)
{
    return float2(k_s3 * (cell.x + 0.5 * (cell.y & 1)), 1.5 * cell.y);
}

// The nearest tile centre: the row above or below, each at its rounded column.
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

float4 fetch(int2 cell, int w, int h)
{
    if (cell.y < 0 || cell.y >= h)
        return float4(0, 0, 0, 0);
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
    const float4 own = fetch(cell, (int)w, (int)h);

    float4 col = own;
    const float band = u_params.x;
    if (band > 0.0)
    {
        // The side the pixel is nearest: the direction of the offset from the
        // centre, snapped to the six neighbour directions (0, 60, ... degrees).
        const float2 v = p - centre_of(cell);
        const float a = atan2(v.y, v.x);
        const float k = round(a / 1.0471975511965976);
        const float2 dir = float2(cos(k * 1.0471975511965976), sin(k * 1.0471975511965976));
        const float d = 0.5 * k_s3 - dot(v, dir); // distance to that side, >= 0 inside
        const float4 nb = fetch(nearest_cell(centre_of(cell) + k_s3 * dir), (int)w, (int)h);
        const float t = 0.5 - 0.5 * smoothstep(0.0, band, d); // the neighbour's share
        const float3 pm = lerp(own.rgb * own.a, nb.rgb * nb.a, t);
        const float  pa = lerp(own.a, nb.a, t);
        col = float4(pa > 0.0 ? pm / pa : float3(0, 0, 0), pa);
    }
    if (col.a <= 0.0)
        discard;
    output.o_color = col * input.v_color;
    return output;
}
