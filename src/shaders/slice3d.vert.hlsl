// slice3d (vertex) - BL-1665 (3D slice): the ground as a heightmesh, measured beside the
// fixed-angle ground (TECH_FOUNDATIONS.md § Rendering dimension, "Toward 3D, in two steps").
// A world position (x east, y up, z south; canonical units), the bake's uv and a normal in;
// clip position out, and the position in the sun's clip space for the shadow map. The
// shadow pass draws with this same shader, the sun's matrix as u_mvp. SDL GPU's DXIL
// conventions: vertex uniforms at b0 space1, vertex inputs as TEXCOORD<location>.

cbuffer slice_vs : register(b0, space1)
{
    float4x4 u_mvp;   // column-major, applied as mul(u_mvp, p)
    float4x4 u_sun;   // the sun's view-projection (the shadow map's)
};

struct VSIn
{
    float3 pos : TEXCOORD0;
    float2 uv  : TEXCOORD1;
    float3 nrm : TEXCOORD2;
};

struct VSOut
{
    float4 pos  : SV_Position;
    float2 uv   : TEXCOORD0;
    float3 nrm  : TEXCOORD1;
    float3 wpos : TEXCOORD2;
    float4 spos : TEXCOORD3;
};

VSOut main(VSIn i)
{
    VSOut o;
    o.pos  = mul(u_mvp, float4(i.pos, 1.0));
    o.uv   = i.uv;
    o.nrm  = i.nrm;
    o.wpos = i.pos;
    o.spos = mul(u_sun, float4(i.pos, 1.0));
    return o;
}
