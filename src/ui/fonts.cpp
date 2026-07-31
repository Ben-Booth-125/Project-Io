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

    // Glyph range (BL-234). Without this ImGui builds its default atlas — Basic
    // Latin + Latin-1 only, U+0020..U+00FF — and every codepoint above renders as
    // the not-found glyph "?". That inverts the meaning of the bare em-dash
    // placeholders in entity_summary.cpp, which read as "the app does not know"
    // rather than "nothing is selected".
    //
    // Three blocks past Latin-1 are actually used in UI string literals; a survey
    // of src/ found 43 sites across 7 codepoints:
    //   General Punctuation  — (U+2014, 37 sites) – …
    //   Arrows               ← ↑ → ↓ (U+2190..U+2193, the nav hints in app.cpp)
    //   Mathematical Ops     ≥ (U+2265) ≈ (U+2248)
    // The ranges are widened to whole blocks rather than the exact seven, so the
    // next em-dash sibling someone types does not silently regress this.
    //
    // Static storage is required: ImGui keeps the pointer and reads the array
    // later, when the atlas is built — not here.
    //
    // The bundled DejaVuSans covers all three blocks (verified against its cmap:
    // General Punctuation 107/112, Arrows 112/112, Mathematical Operators
    // 256/256), so nothing here asks the font for a glyph it does not have.
    static constexpr ImWchar k_glyph_ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin-1 Supplement (ImGui's default)
        0x2000, 0x206F, // General Punctuation — em/en dash, ellipsis, quotes
        0x2190, 0x21FF, // Arrows
        0x2200, 0x22FF, // Mathematical Operators
        0,
    };
    cfg.GlyphRanges = k_glyph_ranges;

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
    // pixel font can. SizePixels left at ProggyClean's native 13. The glyph range
    // is cleared first — ProggyClean is Latin-1 only, so asking it for the three
    // extra blocks would build atlas space for glyphs it cannot supply.
    cfg.GlyphRanges = nullptr;
    cfg.SizePixels  = 13.0f;
    return io.Fonts->AddFontDefault(&cfg);
}

ImFont* reload_ui_font(float size_px)
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    return load_ui_font(size_px);
}

} // namespace ui
