// ground_lift - the ground images, with the land lift applied at draw time (BL-1659
// (live grade shader); RENDERING.md § Art direction and palette, "The land lift").
//
// The draw's own texture is a ground chunk, baked WITHOUT the lift. The lift is the
// bake's apply_land_lift moved here: a gamma on luminance applied as a ratio of the
// pixel's own channels, held where it would clip a channel, weighted by land. The
// LAND WEIGHT is per tile (NR-1226, option D): a table of one texel per tile at
// sampler slot 1, r = 1 on a land tile the bake draws as land. The tile is found
// from the pixel's SCREEN position through the canvas's inverse camera, so a tile
// boundary is a hex side, softened over a short band - not the painted coast. A
// river's water on a land tile lifts with its tile.
#include "common.hlsli"

Texture2D    u_land         : register(t1, space2);
SamplerState u_land_sampler : register(s1, space2);

cbuffer lift_params : register(b0, space3)
{
    float4 u_cam;  // canonical x = cam.x * px + cam.y, canonical y = cam.z * py + cam.w
    float4 u_lift; // x: gamma (1 = off), y: edge band, canonical units
};

// SDL's vertex shader emits the screen position after the colour and the UV.
struct PSLiftInput {
    float4 v_color : TEXCOORD0;
    float2 v_uv    : TEXCOORD1;
    float4 v_pos   : SV_Position;
};

static const float k_s3 = 1.7320508075688772;

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

float land_of(int2 cell, int w, int h)
{
    if (cell.y < 0 || cell.y >= h)
        return 0.0;
    const int c = ((cell.x % w) + w) % w;
    return u_land.Load(int3(c, cell.y, 0)).r;
}

PSOutput main(PSLiftInput input)
{
    PSOutput output;
    float4 tex = u_texture.Sample(u_sampler, input.v_uv);
    const float gamma = u_lift.x;
    if (gamma < 1.0)
    {
        uint w, h;
        u_land.GetDimensions(w, h);
        const float2 p = float2(u_cam.x * input.v_pos.x + u_cam.y, u_cam.z * input.v_pos.y + u_cam.w);
        const int2 cell = nearest_cell(p);
        float land = land_of(cell, (int)w, (int)h);
        const float band = u_lift.y;
        if (band > 0.0)
        {
            const float2 v = p - centre_of(cell);
            const float k = round(atan2(v.y, v.x) / 1.0471975511965976);
            const float2 dir = float2(cos(k * 1.0471975511965976), sin(k * 1.0471975511965976));
            const float d = 0.5 * k_s3 - dot(v, dir);
            const float nb = land_of(nearest_cell(centre_of(cell) + k_s3 * dir), (int)w, (int)h);
            land = lerp(land, nb, 0.5 - 0.5 * smoothstep(0.0, band, d));
        }
        if (land > 0.0)
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
    output.o_color = tex * input.v_color;
    return output;
}
