#include "fonts.hpp"

#include <imgui.h>

#include <array>
#include <filesystem>

namespace ui {

ImFont* load_ui_font(float size_px)
{
    ImGuiIO& io = ImGui::GetIO();

    ImFontConfig cfg;
    cfg.OversampleH = 3;     // horizontal sub-pixel sampling — removes the label shimmer
    cfg.OversampleV = 1;
    cfg.PixelSnapH  = false; // allow fractional horizontal placement so motion stays fluid

    // System font candidates, most-preferred first. A bundled font under
    // assets/fonts can be prepended here later for cross-platform builds.
    static constexpr std::array<const char*, 4> candidates = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/verdana.ttf",
    };

    for (const char* path : candidates)
    {
        std::error_code ec;
        if (std::filesystem::exists(path, ec))
        {
            if (ImFont* f = io.Fonts->AddFontFromFileTTF(path, size_px, &cfg))
                return f;
        }
    }

    // Fallback: the built-in font, still oversampled so it benefits as far as a
    // pixel font can. SizePixels left at ProggyClean's native 13.
    cfg.SizePixels = 13.0f;
    return io.Fonts->AddFontDefault(&cfg);
}

} // namespace ui
