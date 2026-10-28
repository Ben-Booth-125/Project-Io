#include "unit_art.hpp"

#include "png_decode.hpp"

#include "world/unit_designs.hpp" // kit_slot_count

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace ui {

namespace {

std::array<unit_art, land_class_count> g_art{};

/// The fallback anchors: spread down the image's middle, so a class with no
/// .anchors file still joins each box to a distinct point.
void default_anchors(unit_art& a, int slots)
{
    for (int s = 0; s < 3; ++s)
    {
        a.anchor_x[s] = 0.5f;
        a.anchor_y[s] = slots > 0 ? (static_cast<float>(s) + 0.5f) / static_cast<float>(slots) : 0.5f;
    }
}

void read_anchors(unit_art& a, const std::string& path, int slots)
{
    std::ifstream in(path);
    if (!in)
    {
        SDL_Log("unit_art: no anchors at %s; using the default spread", path.c_str());
        return;
    }
    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        int   slot = -1;
        float x = 0.0f, y = 0.0f;
        std::istringstream ls(line);
        if ((ls >> slot >> x >> y) && slot >= 0 && slot < slots)
        {
            a.anchor_x[slot] = std::clamp(x, 0.0f, 1.0f);
            a.anchor_y[slot] = std::clamp(y, 0.0f, 1.0f);
        }
    }
}

} // namespace

const char* unit_art_stem(land_class c)
{
    switch (c)
    {
        case land_class::infantry:     return "infantry";
        case land_class::motorised:    return "motorised";
        case land_class::mechanised:   return "mechanised";
        case land_class::armour:       return "armour";
        case land_class::light_armour: return "light_armour";
        case land_class::artillery:    return "artillery";
        case land_class::air_defence:  return "air_defence";
        case land_class::none:         break;
    }
    return "";
}

void unit_art_load(SDL_Renderer* r)
{
    unit_art_release();
    for (int c = 0; c < land_class_count; ++c)
    {
        const land_class lc = static_cast<land_class>(c);
        unit_art&        a  = g_art[static_cast<std::size_t>(c)];
        const int        slots = kit_slot_count(lc);
        default_anchors(a, slots);
        const std::string stem = unit_art_stem(lc);
        if (stem.empty())
            continue;
        const std::string base = "assets/units/" + stem;
        read_anchors(a, base + ".anchors", slots);

        std::ifstream f(base + ".png", std::ios::binary);
        if (!f)
        {
            SDL_Log("unit_art: no image at %s.png", base.c_str());
            continue;
        }
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(f),
                                              std::istreambuf_iterator<char>()};
        int w = 0, h = 0;
        std::vector<std::uint8_t> px;
        std::string err;
        if (!png_decode_rgba(bytes, w, h, px, err))
        {
            SDL_Log("unit_art: %s.png not loaded: %s", base.c_str(), err.c_str());
            continue;
        }
        if (r == nullptr)
            continue;
        SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
        if (t == nullptr)
        {
            SDL_Log("unit_art: texture for %s.png failed: %s", base.c_str(), SDL_GetError());
            continue;
        }
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(t, nullptr, px.data(), w * 4);
        a.tex    = t;
        a.width  = w;
        a.height = h;
    }
}

void unit_art_release()
{
    for (unit_art& a : g_art)
    {
        if (a.tex != nullptr)
            SDL_DestroyTexture(static_cast<SDL_Texture*>(a.tex));
        a.tex   = nullptr;
        a.width = a.height = 0;
    }
}

const unit_art& unit_art_for(land_class c)
{
    const int i = static_cast<int>(c);
    return g_art[static_cast<std::size_t>((i >= 0 && i < land_class_count) ? i : land_class_count - 1)];
}

} // namespace ui
