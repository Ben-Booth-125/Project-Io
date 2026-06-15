#include "png_writer.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

/// CRC-32 (PNG polynomial 0xEDB88320), seeded with @p crc and returned without
/// the final XOR so callers can chain; XOR with 0xFFFFFFFF to finalise.
std::uint32_t crc32_update(const std::uint8_t* p, std::size_t n, std::uint32_t crc)
{
    static std::uint32_t table[256];
    static bool          ready = false;
    if (!ready)
    {
        for (std::uint32_t i = 0; i < 256; ++i)
        {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        ready = true;
    }
    for (std::size_t i = 0; i < n; ++i)
        crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}

/// Adler-32 checksum of @p p, as required by the zlib stream trailer.
std::uint32_t adler32_of(const std::uint8_t* p, std::size_t n)
{
    std::uint32_t a = 1, b = 0;
    for (std::size_t i = 0; i < n; ++i)
    {
        a = (a + p[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

void put_u32_be(std::vector<std::uint8_t>& v, std::uint32_t x)
{
    v.push_back(static_cast<std::uint8_t>(x >> 24));
    v.push_back(static_cast<std::uint8_t>((x >> 16) & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xFF));
    v.push_back(static_cast<std::uint8_t>(x & 0xFF));
}

/// Append a PNG chunk: length, 4-char type, data, and the CRC of (type + data).
void write_chunk(std::vector<std::uint8_t>& out, const char type[4],
                 const std::vector<std::uint8_t>& data)
{
    put_u32_be(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t crc_start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    const std::uint32_t crc =
        crc32_update(out.data() + crc_start, out.size() - crc_start, 0xFFFFFFFFu) ^ 0xFFFFFFFFu;
    put_u32_be(out, crc);
}

} // namespace

bool write_png_rgba(const std::string& path, int width, int height,
                    const unsigned char* rgba, int stride_bytes)
{
    if (width <= 0 || height <= 0 || rgba == nullptr)
        return false;

    constexpr int channels = 4;

    // Filtered raw scanlines: each row is prefixed with a filter-type byte (0 =
    // None), as the PNG spec requires before compression.
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(height) * (1 + static_cast<std::size_t>(width) * channels));
    for (int y = 0; y < height; ++y)
    {
        raw.push_back(0);
        const std::uint8_t* row = rgba + static_cast<std::size_t>(y) * stride_bytes;
        raw.insert(raw.end(), row, row + static_cast<std::size_t>(width) * channels);
    }

    // Wrap the raw bytes in a zlib stream made of stored (uncompressed) DEFLATE
    // blocks — each block carries up to 65535 bytes, with the final block flagged.
    std::vector<std::uint8_t> z;
    z.push_back(0x78); // zlib CMF: deflate, 32K window
    z.push_back(0x01); // zlib FLG: no dict, fastest level
    std::size_t pos = 0;
    const std::size_t n = raw.size();
    do
    {
        const std::size_t block = (n - pos > 65535) ? 65535 : (n - pos);
        const bool        last  = (pos + block >= n);
        z.push_back(last ? 1 : 0); // BFINAL in bit 0, BTYPE 00 (stored)
        const std::uint16_t len  = static_cast<std::uint16_t>(block);
        const std::uint16_t nlen = static_cast<std::uint16_t>(~len);
        z.push_back(len & 0xFF);
        z.push_back((len >> 8) & 0xFF);
        z.push_back(nlen & 0xFF);
        z.push_back((nlen >> 8) & 0xFF);
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + block);
        pos += block;
    } while (pos < n);
    put_u32_be(z, adler32_of(raw.data(), raw.size()));

    // Assemble the file: signature, IHDR, IDAT, IEND.
    std::vector<std::uint8_t> out;
    const std::uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    out.insert(out.end(), sig, sig + 8);

    std::vector<std::uint8_t> ihdr;
    put_u32_be(ihdr, static_cast<std::uint32_t>(width));
    put_u32_be(ihdr, static_cast<std::uint32_t>(height));
    ihdr.push_back(8); // bit depth
    ihdr.push_back(6); // colour type 6 = RGBA
    ihdr.push_back(0); // compression: deflate
    ihdr.push_back(0); // filter method 0
    ihdr.push_back(0); // no interlace
    write_chunk(out, "IHDR", ihdr);

    write_chunk(out, "IDAT", z);

    const std::vector<std::uint8_t> empty;
    write_chunk(out, "IEND", empty);

    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f)
        return false;
    const std::size_t written = std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    return written == out.size();
}

namespace {

/// Read a whole file into a byte vector. Returns false on a missing/unreadable file.
bool slurp_file(const std::string& path, std::vector<std::uint8_t>& out)
{
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size < 0)
    {
        std::fclose(f);
        return false;
    }
    out.resize(static_cast<std::size_t>(size));
    const std::size_t read = out.empty() ? 0 : std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    return read == out.size();
}

std::uint32_t read_u32_be(const std::uint8_t* p)
{
    return (static_cast<std::uint32_t>(p[0]) << 24)
         | (static_cast<std::uint32_t>(p[1]) << 16)
         | (static_cast<std::uint32_t>(p[2]) << 8)
         |  static_cast<std::uint32_t>(p[3]);
}

/// Inflate a zlib stream made only of *stored* (uncompressed) DEFLATE blocks —
/// the exact form write_png_rgba emits. Returns false on any compressed block or
/// a malformed stream, since this reader deliberately decodes nothing else.
bool inflate_stored(const std::uint8_t* z, std::size_t n, std::vector<std::uint8_t>& out)
{
    if (n < 2)
        return false;
    std::size_t pos = 2; // skip the 2-byte zlib header (CMF, FLG)
    while (pos < n)
    {
        const std::uint8_t header = z[pos++];
        const bool         last   = (header & 0x01) != 0;
        const int          btype  = (header >> 1) & 0x03;
        if (btype != 0) // 00 = stored; anything else is unsupported
            return false;
        if (pos + 4 > n)
            return false;
        const std::uint16_t len  = static_cast<std::uint16_t>(z[pos] | (z[pos + 1] << 8));
        const std::uint16_t nlen = static_cast<std::uint16_t>(z[pos + 2] | (z[pos + 3] << 8));
        pos += 4;
        if (static_cast<std::uint16_t>(~len) != nlen)
            return false;
        if (pos + len > n)
            return false;
        out.insert(out.end(), z + pos, z + pos + len);
        pos += len;
        if (last)
            break;
    }
    return true;
}

} // namespace

bool read_png_rgba(const std::string& path, std::vector<unsigned char>& out_rgba,
                   int& out_width, int& out_height)
{
    std::vector<std::uint8_t> bytes;
    if (!slurp_file(path, bytes))
        return false;

    const std::uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (bytes.size() < 8 || std::memcmp(bytes.data(), sig, 8) != 0)
        return false;

    int width = 0, height = 0;
    bool have_ihdr = false;
    std::vector<std::uint8_t> idat; // concatenated IDAT payloads

    std::size_t pos = 8;
    while (pos + 8 <= bytes.size())
    {
        const std::uint32_t length = read_u32_be(&bytes[pos]);
        const char* type = reinterpret_cast<const char*>(&bytes[pos + 4]);
        const std::size_t data_off = pos + 8;
        if (data_off + length + 4 > bytes.size())
            return false; // truncated chunk (data + trailing CRC)

        if (std::memcmp(type, "IHDR", 4) == 0)
        {
            if (length < 13)
                return false;
            width  = static_cast<int>(read_u32_be(&bytes[data_off]));
            height = static_cast<int>(read_u32_be(&bytes[data_off + 4]));
            const std::uint8_t bit_depth   = bytes[data_off + 8];
            const std::uint8_t colour_type = bytes[data_off + 9];
            const std::uint8_t interlace   = bytes[data_off + 12];
            // Only the writer's own format: 8-bit RGBA, no interlace.
            if (bit_depth != 8 || colour_type != 6 || interlace != 0)
                return false;
            have_ihdr = true;
        }
        else if (std::memcmp(type, "IDAT", 4) == 0)
        {
            idat.insert(idat.end(), &bytes[data_off], &bytes[data_off] + length);
        }
        else if (std::memcmp(type, "IEND", 4) == 0)
        {
            break;
        }

        pos = data_off + length + 4; // advance past data and the 4-byte CRC
    }

    if (!have_ihdr || width <= 0 || height <= 0)
        return false;

    std::vector<std::uint8_t> raw;
    if (!inflate_stored(idat.data(), idat.size(), raw))
        return false;

    // raw is filtered scanlines: a filter-type byte per row followed by the pixels.
    constexpr int channels = 4;
    const std::size_t row_bytes = static_cast<std::size_t>(width) * channels;
    const std::size_t expected  = static_cast<std::size_t>(height) * (1 + row_bytes);
    if (raw.size() != expected)
        return false;

    out_rgba.resize(static_cast<std::size_t>(height) * row_bytes);
    for (int y = 0; y < height; ++y)
    {
        const std::size_t src = static_cast<std::size_t>(y) * (1 + row_bytes);
        if (raw[src] != 0) // only filter type 0 (None) — the writer's choice
            return false;
        std::memcpy(out_rgba.data() + static_cast<std::size_t>(y) * row_bytes,
                    raw.data() + src + 1, row_bytes);
    }

    out_width  = width;
    out_height = height;
    return true;
}

float diff_rgba(const unsigned char* a, const unsigned char* b,
                int width, int height, int threshold,
                const std::string& diff_path)
{
    if (a == nullptr || b == nullptr || width <= 0 || height <= 0)
        return -1.0f;

    const std::size_t pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    std::vector<std::uint8_t> diff_img;
    const bool want_diff = !diff_path.empty();
    if (want_diff)
        diff_img.resize(pixels * 4);

    std::size_t differing = 0;
    for (std::size_t i = 0; i < pixels; ++i)
    {
        const std::size_t o = i * 4;
        const int dr = std::abs(static_cast<int>(a[o + 0]) - static_cast<int>(b[o + 0]));
        const int dg = std::abs(static_cast<int>(a[o + 1]) - static_cast<int>(b[o + 1]));
        const int db = std::abs(static_cast<int>(a[o + 2]) - static_cast<int>(b[o + 2]));
        const bool differs = std::max(dr, std::max(dg, db)) > threshold;
        if (differs)
            ++differing;

        if (want_diff)
        {
            if (differs)
            {
                diff_img[o + 0] = 255; diff_img[o + 1] = 0; diff_img[o + 2] = 255; // magenta flag
            }
            else
            {
                // Dimmed copy of `a` so the flagged pixels stand out.
                diff_img[o + 0] = static_cast<std::uint8_t>(a[o + 0] / 3);
                diff_img[o + 1] = static_cast<std::uint8_t>(a[o + 1] / 3);
                diff_img[o + 2] = static_cast<std::uint8_t>(a[o + 2] / 3);
            }
            diff_img[o + 3] = 255;
        }
    }

    if (want_diff)
        write_png_rgba(diff_path, width, height, diff_img.data(), width * 4);

    return static_cast<float>(differing) / static_cast<float>(pixels);
}
