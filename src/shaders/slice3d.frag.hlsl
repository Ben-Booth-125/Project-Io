// slice3d (fragment) - BL-1665 (3D slice): the bake as the heightmesh's colour, lit by the
// sun, with cast shadows from a shadow map. The bake already carries its own slope shading
// from its fixed light, so the mesh's light applies as a ratio against flat ground under
// that same light. The land lift (a gamma on luminance) is applied as the ground shader
// applies it. Distant ground fades toward the sky. SDL GPU's DXIL conventions: fragment
// samplers at t<n>/s<n> space2, fragment uniforms at b0 space3.

Texture2D    u_bake           : register(t0, space2);
SamplerState u_bake_sampler   : register(s0, space2);
Texture2D    u_shadow         : register(t1, space2);
SamplerState u_shadow_sampler : register(s1, space2);

cbuffer slice_fs : register(b0, space3)
{
    float4 u_light;  // xyz: toward the sun (world, normalised); w: the lift gamma (1 = off)
    float4 u_sky;    // rgb: the haze colour; w: where the haze starts (canonical units)
    float4 u_eye;    // xyz: the camera; w: the bake's own light's height (its y), for the ratio
    float4 u_shade;  // x: shadow depth (0..1, 0 off), y: depth bias, z: one shadow texel (uv)
};

struct PSIn
{
    float4 pos  : SV_Position;
    float2 uv   : TEXCOORD0;
    float3 nrm  : TEXCOORD1;
    float3 wpos : TEXCOORD2;
    float4 spos : TEXCOORD3;
};

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

float4 main(PSIn i) : SV_Target0
{
    float3 c = u_bake.Sample(u_bake_sampler, i.uv).rgb;
    const float3 n = normalize(i.nrm);
    const float3 L = u_light.xyz;
    const float ndl  = dot(n, L);
    const float lit  = max(ndl, 0.0) * 0.85 + 0.15;
    const float flat_ = max(u_eye.w, 0.0) * 0.85 + 0.15;
    c *= clamp(lit / flat_, 0.25, 2.0);
    if (u_shade.x > 0.0)
        c *= 1.0 - u_shade.x * shadow_at(i.spos, ndl);
    if (u_light.w < 1.0)
    {
        const float y = dot(c, float3(0.2126, 0.7152, 0.0722));
        float sc = pow(clamp(y, 1.0 / 255.0, 1.0), u_light.w - 1.0);
        const float mx = max(c.r, max(c.g, c.b));
        if (mx * sc > 1.0)
            sc = max(1.0, 1.0 / max(mx, 1.0 / 255.0));
        c *= sc;
    }
    const float d = length(i.wpos - u_eye.xyz);
    c = lerp(c, u_sky.rgb, 0.5 * saturate((d - u_sky.w) / (3.0 * u_sky.w)));
    return float4(saturate(c), 1.0);
}
