// drape (fragment) - THE 3D GROUND's ground marks, laid over its frame (RENDERING.md § The 3D
// ground, "Ground marks are draped"). The draw's own texture (slot 0) is the 3D pass's
// coordinate image: for each screen pixel, where in the drape that pixel's ground lies,
// 16 bits an axis (r.g the u, b.a the v; zero where no ground is). The drape itself is the
// extra sampler at slot 1, premultiplied. Drawn in SDL's own queue, after the drape, so the
// marks are this frame's - the 3D pass runs on a command buffer that reaches the GPU first,
// and sampling the drape there showed the previous frame's.
#include "common.hlsli"

Texture2D    u_drape         : register(t1, space2);
SamplerState u_drape_sampler : register(s1, space2);

PSOutput main(PSInput input) {
    PSOutput output;
    const uint4 b = (uint4)round(u_texture.Sample(u_sampler, input.v_uv) * 255.0);
    const uint qu = b.r * 256u + b.g, qv = b.b * 256u + b.a;
    if (qu == 0u || qv == 0u)
    {
        output.o_color = float4(0, 0, 0, 0);
        return output;
    }
    const float2 duv = (float2(qu, qv) - 1.0) / 65534.0;
    output.o_color = u_drape.SampleLevel(u_drape_sampler, duv, 0) * input.v_color.a;
    return output;
}
