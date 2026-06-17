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

    // Font candidates, most-preferred first.
    //
    // The bundled DejaVuSans (copied next to the executable from assets/ by the
    // CMake POST_BUILD step, loaded cwd-relative exactly like scripts/) is tried
    // first so on-screen text — and any visual golden-diff capture — renders
    // identically on every OS and machine, independent of which system fonts are
    // installed. System fonts follow as fallbacks (Linux then Windows) for the
    // rare build without the bundled asset; ProggyClean is the final backstop.
    static constexpr std::array<const char*, 8> candidates = {
        // Bundled — deterministic across platforms (see BACKLOG BL-057).
        "assets/fonts/DejaVuSans.ttf",
        // Linux system fonts.
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        // Windows system fonts.
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
