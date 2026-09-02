#include "terrain_palette.hpp"

#include <algorithm>
#include <cmath>

// Every function body here is the hex_render.cpp original, moved verbatim
// (BL-732): the visual goldens compare pixels, so the extraction must be
// arithmetic-identical, float op for float op.

namespace ui::palette {

colour32 blend(colour32 a, colour32 b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float ar = static_cast<float>(col_r(a));
    const float ag = static_cast<float>(col_g(a));
    const float ab = static_cast<float>(col_b(a));
    const float aa = static_cast<float>(col_a(a));
    const float br = static_cast<float>(col_r(b));
    const float bg = static_cast<float>(col_g(b));
    const float bb = static_cast<float>(col_b(b));
    return col32(static_cast<int>(ar + (br - ar) * t),
                 static_cast<int>(ag + (bg - ag) * t),
                 static_cast<int>(ab + (bb - ab) * t),
                 static_cast<int>(aa));
}

colour32 blend_round(colour32 a, colour32 b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    auto ch = [&](int shift)
    {
        const float av = static_cast<float>((a >> shift) & 0xFF);
        const float bv = static_cast<float>((b >> shift) & 0xFF);
        return static_cast<int>(std::lround(av + (bv - av) * t));
    };
    return col32(ch(k_r_shift), ch(k_g_shift), ch(k_b_shift), col_a(a));
}

colour32 substrate_colour(terrain_substrate sub)
{
    switch (sub)
    {
        case terrain_substrate::barren:   return col32(170, 145, 100, 255);
        case terrain_substrate::rocky:    return col32(112, 105,  95, 255);
        case terrain_substrate::volcanic: return col32(135,  55,  28, 255);
        case terrain_substrate::icy:      return col32(200, 224, 236, 255);
        case terrain_substrate::ocean:    return col32( 40,  80, 160, 255);
        case terrain_substrate::coast:    return col32( 78, 128, 194, 255);
        case terrain_substrate::lake:     return col32( 62, 122, 172, 255);
        case terrain_substrate::regolith: return col32(138, 130, 120, 255);
        case terrain_substrate::metallic: return col32(158, 150, 140, 255);
        case terrain_substrate::sedimentary: return col32(150, 138, 110, 255);
    }
    return col32(60, 60, 60, 255);
}

colour32 cover_endpoint(terrain_cover c)
{
    switch (c)
    {
        case terrain_cover::grass:  return col32( 58, 158,  45, 255);
        case terrain_cover::forest: return col32( 23,  93,  43, 255);
        case terrain_cover::marsh:  return col32( 42, 111,  83, 255);
        case terrain_cover::scrub:  return col32(116, 145, 137, 255);
        case terrain_cover::snow:   return col32(240, 246, 252, 255);
        case terrain_cover::dunes:  return col32(214, 190, 138, 255);
        case terrain_cover::ash:    return col32( 78,  72,  70, 255);
        case terrain_cover::salt:   return col32(232, 228, 216, 255);
        case terrain_cover::urban:  return col32(120, 118, 128, 255);
        case terrain_cover::none:   break;
    }
    return col32(0, 0, 0, 0);
}

colour32 tile_colour(terrain_substrate sub, terrain_cover cov, std::uint8_t density)
{
    const colour32 base = substrate_colour(sub);
    if (cov == terrain_cover::none || density == 0)
        return base;
    if (cov == terrain_cover::urban)
        return cover_endpoint(terrain_cover::urban);
    return blend_round(base, cover_endpoint(cov), cover_fraction(density));
}

float relief_amount(terrain_landform lf)
{
    switch (lf)
    {
        case terrain_landform::mountain: return  0.22f;
        case terrain_landform::highland: return  0.12f;
        case terrain_landform::plains:   return  0.00f;
        case terrain_landform::crater:   return -0.08f;
        case terrain_landform::valley:   return -0.10f;
        case terrain_landform::rift:     return -0.14f;
        case terrain_landform::canyon:   return -0.16f;
    }
    return 0.0f;
}

colour32 landform_relief(colour32 base, terrain_landform lf)
{
    const float a = relief_amount(lf);
    if (a == 0.0f)
        return base;
    constexpr colour32 highlight = col32(255, 248, 225, 255);
    constexpr colour32 shadow    = col32( 18,  24,  40, 255);
    return blend(base, a > 0.0f ? highlight : shadow, a > 0.0f ? a : -a);
}

} // namespace ui::palette
