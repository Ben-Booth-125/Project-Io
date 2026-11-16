// slice3d_shadow (fragment) - the 3D ground's shadow pass writes depth only, so its
// fragment stage does nothing.

struct PSIn
{
    float4 pos  : SV_Position;
    float2 uv   : TEXCOORD0;
    float3 nrm  : TEXCOORD1;
    float3 wpos : TEXCOORD2;
    float4 spos : TEXCOORD3;
    float4 col  : TEXCOORD4;
};

void main(PSIn i)
{
}
