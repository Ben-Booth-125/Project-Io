// Shared bindings for Io's fragment shaders on SDL's 2D GPU renderer (BL-1655 (shader seam)).
//
// SDL's GPU renderer draws every textured triangle with its own vertex shader
// (tri_texture.vert): it hands the fragment stage a colour and a UV, and binds the
// draw's texture at sampler slot 0. Fragment resources live in register space 2 and
// fragment uniforms in space 3 - the SDL_GPU convention for DXIL. A render state's
// extra samplers (SDL_GPURenderStateCreateInfo::sampler_bindings) follow slot 0.

Texture2D    u_texture : register(t0, space2);
SamplerState u_sampler : register(s0, space2);

// The SEMANTICS must be the ones SDL's compiled vertex shader emits, not the ones its
// HLSL source spells: SDL builds its shaders through SDL_shadercross (HLSL -> SPIR-V ->
// DXIL), which renames every interstage value TEXCOORDn in order. So the colour arrives
// as TEXCOORD0 and the UV as TEXCOORD1; spelled COLOR0/TEXCOORD0, D3D12 refuses the
// pipeline (the signatures do not link) and SDL silently skips the draw.
struct PSInput {
    float4 v_color : TEXCOORD0;
    float2 v_uv    : TEXCOORD1;
};

struct PSOutput {
    float4 o_color : SV_Target;
};
