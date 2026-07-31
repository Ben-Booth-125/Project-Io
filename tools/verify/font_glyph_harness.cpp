// Font glyph-range harness (BL-234). Verifies that the atlas ui::load_ui_font
// builds actually carries every codepoint the UI's string literals use.
//
// Unlike its neighbours in tools/verify/, this one links ImGui — it has to,
// because the defect it guards lives in the ImFontConfig handed to
// AddFontFromFileTTF, not in world/* logic. It still links no SDL and no Lua:
// ImGui's font atlas is built entirely on the CPU, with no renderer, no window
// and no backend, so the check runs headless in milliseconds.
//
// THE DEFECT IT GUARDS. Without an explicit GlyphRanges, ImGui builds its
// default atlas — Basic Latin + Latin-1, U+0020..U+00FF — and every codepoint
// above renders as "?". That silently inverts meaning: the bare em-dash
// placeholders in entity_summary.cpp mean "nothing is selected", but drew a lone
// "?", which reads as "the app does not know".
//
// This is a REGRESSION guard, not a one-off. The failure mode is invisible in
// code review (the string literal looks perfect) and only shows on a rendered
// frame, so it is exactly the class worth pinning with an assertion.
//
// Build (from the repo root; no SDL, no Lua):
//   cmake --build <build-dir> --target font_glyph_harness
//
// By hand, where <imgui> is the fetched ImGui source directory, compile these
// together with -std=c++20 -I src -I src/ui -I <imgui> :
//   tools/verify/font_glyph_harness.cpp, src/ui/fonts.cpp, and ImGui's
//   imgui.cpp, imgui_draw.cpp, imgui_tables.cpp, imgui_widgets.cpp

#include <imgui.h>

#include "ui/fonts.hpp"

#include <cstdio>

namespace {

int g_failures = 0;

void check(bool cond, const char* what)
{
    if (!cond) ++g_failures;
    std::printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
}

/// One probed codepoint: what it is, and why the UI needs it.
struct glyph_probe
{
    unsigned    codepoint;
    const char* what;
};

// The codepoints above Latin-1 that src/ string literals actually contain, as of
// a 2026-07-31 survey: 43 sites across 7 codepoints, in three Unicode blocks.
// The two Latin-1 controls at the end must pass both before AND after the fix —
// they are what proves a failing run is a real narrowing and not a broken probe.
constexpr glyph_probe k_probes[] = {
    // General Punctuation (U+2000..U+206F) — the bulk of the sites.
    {0x2014, "U+2014 em-dash — 37 UI string sites, incl. the bare placeholders"},
    {0x2013, "U+2013 en-dash — block cover"},
    {0x2026, "U+2026 ellipsis — block cover"},
    // Arrows (U+2190..U+21FF) — the nav hints in app.cpp's keybinding table.
    {0x2190, "U+2190 left arrow — nav hint"},
    {0x2191, "U+2191 up arrow — nav hint"},
    {0x2192, "U+2192 right arrow — nav hint"},
    {0x2193, "U+2193 down arrow — nav hint"},
    // Mathematical Operators (U+2200..U+22FF).
    {0x2265, "U+2265 greater-or-equal — the BL-069 workforce tooltip"},
    {0x2248, "U+2248 almost-equal — corp_ai"},
    // Latin-1 controls: in range even before the fix. If either of these fails,
    // the harness itself is broken, not the font range.
    {0x00B1, "U+00B1 plus-minus — Latin-1 CONTROL (must pass regardless)"},
    {0x0041, "U+0041 letter A — Latin-1 CONTROL (must pass regardless)"},
};

} // namespace

int main()
{
    std::printf("BL-234 font glyph-range harness\n");

    ImGui::CreateContext();
    ImGuiIO& io  = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280.0f, 720.0f);

    // The real production path — not a reimplementation of it. If load_ui_font
    // stops requesting the ranges, this harness fails.
    ImFont* font = ui::load_ui_font(16.0f);
    check(font != nullptr, "R1 ui::load_ui_font returns a font");
    if (font == nullptr)
    {
        std::printf("\nFAILURES (%d)\n", ++g_failures);
        return 1;
    }
    io.Fonts->Build();

    // FindGlyphNoFallback, not FindGlyph: the latter substitutes the fallback
    // glyph and would report success for every codepoint, passing vacuously.
    for (const glyph_probe& p : k_probes)
        check(font->FindGlyphNoFallback(static_cast<ImWchar>(p.codepoint)) != nullptr, p.what);

    std::printf("\n%s  (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
