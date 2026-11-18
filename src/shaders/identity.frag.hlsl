// The seam's proof (BL-1655 (shader seam)): exactly what SDL's own textured-geometry
// shader draws - the texture times the vertex colour - so a frame drawn through it
// matches a frame drawn without it. Kept as the seam's standing self-check.
#include "common.hlsli"

PSOutput main(PSInput input) {
    PSOutput output;
    output.o_color = u_texture.Sample(u_sampler, input.v_uv) * input.v_color;
    return output;
}
