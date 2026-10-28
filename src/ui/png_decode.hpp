#pragma once

// ---------------------------------------------------------------------------
// A small PNG decoder for the UI's authored images (the unit designer's class
// art, docs/ui/ledgers/military.md § The designer).
//
// The app links no image library: SDL 3.2 loads BMP only, and ImGui bundles no
// stb_image. Rather than add a dependency for a handful of authored files, this
// decodes the subset an art export produces: 8-bit greyscale, grey+alpha, RGB and
// RGBA, and 8-bit palette, non-interlaced, every filter type, with a complete
// zlib inflate (stored, fixed and dynamic Huffman blocks). Anything else is
// refused with a reason rather than drawn wrong. Pure: no SDL, no ImGui.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <string>
#include <vector>

namespace ui {

/// Decode the PNG in @p file to tightly packed RGBA8 (row-major, top row first).
/// Returns false, with @p err set, for a file outside the supported subset or a
/// corrupt stream.
bool png_decode_rgba(const std::vector<std::uint8_t>& file, int& width, int& height,
                     std::vector<std::uint8_t>& rgba, std::string& err);

} // namespace ui
