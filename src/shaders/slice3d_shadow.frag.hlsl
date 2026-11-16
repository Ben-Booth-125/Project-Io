// slice3d_shadow (fragment) - BL-1665 (3D slice): the shadow pass writes depth only, so
// its fragment stage does nothing.

struct PSIn
{
    float4 pos  : SV_Position;
    float2 uv   : TEXCOORD0;
    float3 nrm  : TEXCOORD1;
    float3 wpos : TEXCOORD2;
    float4 spos : TEXCOORD3;
};

void main(PSIn i)
{
}
