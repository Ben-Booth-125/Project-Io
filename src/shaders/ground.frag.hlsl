// ground - the ground images, with the land lift and the close-zoom detail applied at
// draw time (RENDERING.md § Art direction and palette, "The lift at draw time", BL-1659
// (live grade shader); § Level of detail, "The two closest rungs add detail at draw
// time", BL-1658 (close-zoom detail layer)).
//
// The draw's own texture is a ground chunk, baked WITHOUT the lift and without detail.
//   - The TILE TABLE (slot 1, one texel per tile, read by Load): r = 1 on a land tile the
//     bake draws as land; g = its detail family (0..3, as g * 255).
//   - The DETAIL texture (slot 2, linear, repeating): four seamless greyscale textures,
//     one per family, in r, g, b, a (vegetation, forest, arid, rock), mean 0.5.
// Each pixel finds its tile from its SCREEN position through the canvas's inverse camera,
// so a tile boundary is a hex side, softened over a short band toward the neighbour across
// the nearest side. Detail first (a luminance ratio of +-strength at the texture's
// extremes), then the lift (bake_region's apply_land_lift: a gamma on luminance, applied
// as a ratio of the pixel's own channels, held where it would clip), both weighted by land.
#include "common.hlsli"

Texture2D    u_tiles          : register(t1, space2);
SamplerState u_tiles_sampler  : register(s1, space2);
Texture2D    u_detail         : register(t2, space2);
SamplerState u_detail_sampler : register(s2, space2);

cbuffer ground_params : register(b0, space3)
{
    float4 u_cam;  // canonical x = cam.x * px + cam.y, canonical y = cam.z * py + cam.w
    float4 u_lift; // x: lift gamma (1 = off), y: edge band (canonical units),
                   // z: detail strength (0 = off), w: detail period (canonical units)
};

// SDL's vertex shader emits the screen position after the colour and the UV.
struct PSGroundInput {
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

float2 tile_of(int2 cell, int w, int h) // (land weight, family index)
{
    if (cell.y < 0 || cell.y >= h)
        return float2(0, 0);
    const int c = ((cell.x % w) + w) % w;
    const float4 t = u_tiles.Load(int3(c, cell.y, 0));
    return float2(t.r, round(t.g * 255.0));
}

float family_pick(float4 d, float fam)
{
    return fam < 0.5 ? d.r : fam < 1.5 ? d.g : fam < 2.5 ? d.b : d.a;
}

PSOutput main(PSGroundInput input)
{
    PSOutput output;
    float4 tex = u_texture.Sample(u_sampler, input.v_uv);
    const float gamma = u_lift.x, strength = u_lift.z;
    if (gamma < 1.0 || strength > 0.0)
    {
        uint w, h;
        u_tiles.GetDimensions(w, h);
        const float2 p = float2(u_cam.x * input.v_pos.x + u_cam.y, u_cam.z * input.v_pos.y + u_cam.w);
        const int2 cell = nearest_cell(p);
        const float2 own = tile_of(cell, (int)w, (int)h);
        float2 nb = own;
        float t = 0.0; // the neighbour's share
        const float band = u_lift.y;
        if (band > 0.0)
        {
            const float2 v = p - centre_of(cell);
            const float k = round(atan2(v.y, v.x) / 1.0471975511965976);
            const float2 dir = float2(cos(k * 1.0471975511965976), sin(k * 1.0471975511965976));
            const float d = 0.5 * k_s3 - dot(v, dir);
            nb = tile_of(nearest_cell(centre_of(cell) + k_s3 * dir), (int)w, (int)h);
            t = 0.5 - 0.5 * smoothstep(0.0, band, d);
        }
        const float land = lerp(own.x, nb.x, t);
        if (land > 0.0)
        {
            if (strength > 0.0)
            {
                const float4 dt = u_detail.Sample(u_detail_sampler, p / u_lift.w);
                // Each side's detail counts only where that side is land.
                const float dv = lerp(own.x * (family_pick(dt, own.y) - 0.5),
                                      nb.x * (family_pick(dt, nb.y) - 0.5), t);
                tex.rgb = saturate(tex.rgb * (1.0 + 2.0 * strength * dv));
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
    }
    output.o_color = tex * input.v_color;
    return output;
}
