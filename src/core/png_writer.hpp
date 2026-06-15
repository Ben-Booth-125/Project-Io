#pragma once

#include <string>
#include <vector>

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

/// Read an 8-bit RGBA PNG previously produced by write_png_rgba.
///
/// Only the narrow subset this writer emits is supported — and that is the point:
/// it reads the golden reference images the same harness blessed, not arbitrary
/// PNGs. Specifically: 8-bit depth, colour type 6 (RGBA), no interlace, filter
/// type 0 (None) on every row, and a zlib stream of *stored* (uncompressed)
/// DEFLATE blocks (no Huffman decoding). Any other feature returns false.
///
/// @param path       Input PNG path.
/// @param out_rgba   Receives width*height*4 packed RGBA bytes on success.
/// @param out_width  Receives the image width on success.
/// @param out_height Receives the image height on success.
/// @return true on success; false on a missing/unreadable file or an unsupported
///         PNG feature.
bool read_png_rgba(const std::string& path, std::vector<unsigned char>& out_rgba,
                   int& out_width, int& out_height);

/// Compare two equally-sized, packed (stride = width*4) RGBA buffers.
///
/// A pixel *differs* when the maximum of its per-channel R/G/B absolute deltas
/// exceeds @p threshold (the per-pixel knob `T` — absorbs anti-aliasing and
/// sub-pixel font jitter). The alpha channel is ignored (captures are opaque).
/// The caller decides pass/fail by comparing the returned fraction against its
/// own fraction knob `F`.
///
/// @param a         First image (e.g. the just-captured frame).
/// @param b         Second image (e.g. the golden reference).
/// @param width     Shared image width in pixels.
/// @param height    Shared image height in pixels.
/// @param threshold Per-channel delta above which a pixel counts as differing.
/// @param diff_path When non-empty, a diff image is written there: differing
///                  pixels flagged magenta over a dimmed copy of @p a.
/// @return The fraction of differing pixels in [0, 1]; a negative value if either
///         buffer is null or the dimensions are non-positive.
float diff_rgba(const unsigned char* a, const unsigned char* b,
                int width, int height, int threshold,
                const std::string& diff_path);
