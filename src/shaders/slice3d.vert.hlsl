// slice3d (vertex) - THE 3D GROUND (RENDERING.md § The 3D ground): the ground as a
// heightmesh, and the installations as low-poly solids on it. A position in absolute
// canonical units (x east, y up, z south), the bake's uv, a normal and a colour (alpha 1 on
// a building, which takes its own colour; 0 on the ground, which takes the bake's) in; clip
// position out, the camera-relative world position, and the position in the sun's clip
// space for the shadow map. The shadow pass draws with this same shader, the sun's matrix
// as u_mvp. SDL GPU's DXIL conventions: vertex uniforms at b0 space1, vertex inputs as
// TEXCOORD<location>.

cbuffer slice_vs : register(b0, space1)
{
    float4x4 u_mvp;   // column-major, applied as mul(u_mvp, p)
    float4x4 u_sun;   // the sun's view-projection (the shadow map's)
    float4   u_off;   // xyz: added to the position - the wrap copy, less the camera's origin
};

struct VSIn
{
    float3 pos : TEXCOORD0;
    float2 uv  : TEXCOORD1;
    float3 nrm : TEXCOORD2;
    float4 col : TEXCOORD3;
};

struct VSOut
{
    float4 pos  : SV_Position;
    float2 uv   : TEXCOORD0;
    float3 nrm  : TEXCOORD1;
    float3 wpos : TEXCOORD2;
    float4 spos : TEXCOORD3;
    float4 col  : TEXCOORD4;
};

VSOut main(VSIn i)
{
    VSOut o;
    const float4 p = float4(i.pos + u_off.xyz, 1.0);
    o.pos  = mul(u_mvp, p);
    o.uv   = i.uv;
    o.nrm  = i.nrm;
    o.wpos = p.xyz;
    o.spos = mul(u_sun, p);
    o.col  = i.col;
    return o;
}
