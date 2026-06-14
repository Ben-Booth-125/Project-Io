#include "png_writer.hpp"

#include <cstdint>
#include <cstdio>
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
