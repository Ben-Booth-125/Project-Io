#pragma once

#include <string>

/// Write an 8-bit RGBA image to @p path as a PNG file.
///
/// Dependency-free: the image data is wrapped in a zlib stream built entirely
/// from *stored* (uncompressed) DEFLATE blocks, so no compression library is
/// needed. Files are larger than a compressed PNG but are valid and read by any
/// standard tool. Intended for the visual-verification harness, where captures
/// are inspected, not archived.
///
/// @param path         Output file path (overwritten if it exists).
/// @param width        Image width in pixels (> 0).
/// @param height       Image height in pixels (> 0).
/// @param rgba         Pixel data, 4 bytes/pixel in R,G,B,A byte order.
/// @param stride_bytes Bytes per source row (>= width * 4); rows may be padded.
/// @return true on success; false on a null/empty image or a file-write failure.
bool write_png_rgba(const std::string& path, int width, int height,
                    const unsigned char* rgba, int stride_bytes);
