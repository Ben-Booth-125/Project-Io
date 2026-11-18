// bc1_encode_measure - the BL-1662 (compressed ground textures) spike's measurement
// (Ben, 2026-11-16, NR-1233 option A: "vendor stb_dxt and measure encode ms per 512 px
// chunk; decide the format from the numbers"). A READING, not a check: it asserts nothing.
//
// For each ground image named on the command line (a baked-ground capture, RGBA PNG), it
// cuts every whole 512 x 512 chunk, and for each chunk:
//   - encodes BC1 with stb_dxt (normal and high-quality modes), timing each over several
//     runs and keeping the fastest (ms per chunk);
//   - decodes the blocks back and reports PSNR and the worst per-channel error against the
//     RGB source;
//   - with --out <dir>, writes the decoded chunk as a PNG beside a copy of the source, for
//     an eye comparison.
// Memory per chunk: RGB 768 KiB (the master's opaque format) against BC1 128 KiB.
//
// Standalone (no world, no SDL): build from the repo root with the MSVC environment set -
//   cl /nologo /O2 /EHsc /std:c++20 /I src /I third_party tools\verify\bc1_encode_measure.cpp
//      src\ui\png_decode.cpp /Fe:build_gen\verify\bc1_encode_measure.exe /Fo:build_gen\verify\
//   build_gen\verify\bc1_encode_measure.exe ground_preview_v1_defaults.png [...] [--out dir]
// Ad hoc: not yet a skill (CLAUDE.md § Tool creation is skill creation).

#define STB_DXT_IMPLEMENTATION
#include "stb/stb_dxt.h"

#include "ui/png_decode.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

constexpr int k_chunk = 512;
constexpr int k_runs  = 5;

// ---- BC1 decode (the four-colour mode only, as an opaque encode always emits) ----
void rgb565(std::uint16_t c, int& r, int& g, int& b)
{
    r = ((c >> 11) & 31) * 255 / 31;
    g = ((c >> 5) & 63) * 255 / 63;
    b = (c & 31) * 255 / 31;
}

void decode_block(const std::uint8_t* blk, std::uint8_t out[16][3])
{
    const std::uint16_t c0 = static_cast<std::uint16_t>(blk[0] | (blk[1] << 8));
    const std::uint16_t c1 = static_cast<std::uint16_t>(blk[2] | (blk[3] << 8));
    int pal[4][3];
    rgb565(c0, pal[0][0], pal[0][1], pal[0][2]);
    rgb565(c1, pal[1][0], pal[1][1], pal[1][2]);
    for (int k = 0; k < 3; ++k)
    {
        if (c0 > c1)
        {
            pal[2][k] = (2 * pal[0][k] + pal[1][k]) / 3;
            pal[3][k] = (pal[0][k] + 2 * pal[1][k]) / 3;
        }
        else
        {
            pal[2][k] = (pal[0][k] + pal[1][k]) / 2;
            pal[3][k] = 0;
        }
    }
    const std::uint32_t idx = static_cast<std::uint32_t>(blk[4]) | (blk[5] << 8) | (blk[6] << 16)
                            | (static_cast<std::uint32_t>(blk[7]) << 24);
    for (int i = 0; i < 16; ++i)
    {
        const int s = (idx >> (2 * i)) & 3;
        for (int k = 0; k < 3; ++k)
            out[i][k] = static_cast<std::uint8_t>(pal[s][k]);
    }
}

// ---- a minimal PNG writer (stored deflate blocks), for the eye comparison ----
std::uint32_t crc32(const std::uint8_t* p, std::size_t n, std::uint32_t c = 0xFFFFFFFFu)
{
    for (std::size_t i = 0; i < n; ++i)
    {
        c ^= p[i];
        for (int k = 0; k < 8; ++k)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return c;
}

void put32(std::vector<std::uint8_t>& v, std::uint32_t x)
{
    v.push_back(static_cast<std::uint8_t>(x >> 24)); v.push_back(static_cast<std::uint8_t>(x >> 16));
    v.push_back(static_cast<std::uint8_t>(x >> 8));  v.push_back(static_cast<std::uint8_t>(x));
}

void chunk(std::vector<std::uint8_t>& png, const char* tag, const std::vector<std::uint8_t>& body)
{
    put32(png, static_cast<std::uint32_t>(body.size()));
    std::vector<std::uint8_t> tb(tag, tag + 4);
    tb.insert(tb.end(), body.begin(), body.end());
    png.insert(png.end(), tb.begin(), tb.end());
    put32(png, crc32(tb.data(), tb.size()) ^ 0xFFFFFFFFu);
}

bool write_png_rgb(const std::string& path, int w, int h, const std::vector<std::uint8_t>& rgb)
{
    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(h) * (1 + 3 * w));
    for (int y = 0; y < h; ++y)
    {
        raw.push_back(0);
        raw.insert(raw.end(), rgb.begin() + static_cast<std::ptrdiff_t>(y) * 3 * w,
                   rgb.begin() + static_cast<std::ptrdiff_t>(y + 1) * 3 * w);
    }
    std::vector<std::uint8_t> z = { 0x78, 0x01 };
    std::uint32_t a = 1, b = 0;
    for (std::uint8_t c : raw) { a = (a + c) % 65521u; b = (b + a) % 65521u; }
    for (std::size_t off = 0; off < raw.size() || off == 0;)
    {
        const std::size_t n = std::min<std::size_t>(65535, raw.size() - off);
        const bool last = off + n >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(static_cast<std::uint8_t>(n)); z.push_back(static_cast<std::uint8_t>(n >> 8));
        z.push_back(static_cast<std::uint8_t>(~n)); z.push_back(static_cast<std::uint8_t>((~n) >> 8));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(off),
                 raw.begin() + static_cast<std::ptrdiff_t>(off + n));
        off += n;
        if (last) break;
    }
    put32(z, (b << 16) | a);
    std::vector<std::uint8_t> png = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    std::vector<std::uint8_t> ihdr;
    put32(ihdr, static_cast<std::uint32_t>(w)); put32(ihdr, static_cast<std::uint32_t>(h));
    ihdr.insert(ihdr.end(), { 8, 2, 0, 0, 0 });
    chunk(png, "IHDR", ihdr);
    chunk(png, "IDAT", z);
    chunk(png, "IEND", {});
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    return static_cast<bool>(f);
}

struct reading { double ms = 1e30; double psnr = 0; int max_err = 0; };

// Encodes one chunk (rgba, 512 x 512, row-major) and fills `dec` with the decode.
reading measure(const std::vector<std::uint8_t>& rgba, int mode, std::vector<std::uint8_t>& dec)
{
    reading r;
    const int bw = k_chunk / 4;
    std::vector<std::uint8_t> blocks(static_cast<std::size_t>(bw) * bw * 8);
    for (int run = 0; run < k_runs; ++run)
    {
        const auto t0 = std::chrono::steady_clock::now();
        std::uint8_t src[64];
        for (int by = 0; by < bw; ++by)
            for (int bx = 0; bx < bw; ++bx)
            {
                for (int y = 0; y < 4; ++y)
                    std::memcpy(src + 16 * y,
                                rgba.data() + (static_cast<std::size_t>(by * 4 + y) * k_chunk + bx * 4) * 4, 16);
                for (int i = 0; i < 16; ++i)
                    src[4 * i + 3] = 255; // opaque: the master's chunks carry no alpha
                stb_compress_dxt_block(blocks.data() + (static_cast<std::size_t>(by) * bw + bx) * 8, src, 0, mode);
            }
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        r.ms = std::min(r.ms, ms);
    }
    dec.assign(static_cast<std::size_t>(k_chunk) * k_chunk * 3, 0);
    double se = 0;
    for (int by = 0; by < bw; ++by)
        for (int bx = 0; bx < bw; ++bx)
        {
            std::uint8_t out[16][3];
            decode_block(blocks.data() + (static_cast<std::size_t>(by) * bw + bx) * 8, out);
            for (int i = 0; i < 16; ++i)
            {
                const std::size_t p = static_cast<std::size_t>(by * 4 + i / 4) * k_chunk + bx * 4 + i % 4;
                for (int k = 0; k < 3; ++k)
                {
                    dec[p * 3 + k] = out[i][k];
                    const int e = static_cast<int>(out[i][k]) - static_cast<int>(rgba[p * 4 + k]);
                    se += static_cast<double>(e) * e;
                    r.max_err = std::max(r.max_err, std::abs(e));
                }
            }
        }
    const double mse = se / (static_cast<double>(k_chunk) * k_chunk * 3);
    r.psnr = mse > 0 ? 10.0 * std::log10(255.0 * 255.0 / mse) : 99.0;
    return r;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::string> files;
    std::string out_dir;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc)
            out_dir = argv[++i];
        else
            files.push_back(argv[i]);
    }
    if (files.empty())
    {
        std::fprintf(stderr, "usage: bc1_encode_measure <ground.png>... [--out dir]\n");
        return 2;
    }
    std::printf("BC1 (stb_dxt v1.13) per %d px chunk, fastest of %d runs. RGB %d KiB -> BC1 %d KiB (6:1)\n",
                k_chunk, k_runs, k_chunk * k_chunk * 3 / 1024, k_chunk * k_chunk / 2 / 1024);
    std::printf("%-40s %5s %9s %7s %4s %9s %7s %4s\n", "chunk", "", "normal ms", "PSNR", "max", "HQ ms", "PSNR", "max");
    std::vector<double> ms_n, ms_h, ps_n, ps_h;
    int chunk_id = 0;
    for (const std::string& f : files)
    {
        std::ifstream in(f, std::ios::binary);
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        int w = 0, h = 0;
        std::vector<std::uint8_t> img;
        std::string err;
        if (!ui::png_decode_rgba(bytes, w, h, img, err))
        {
            std::fprintf(stderr, "%s: %s\n", f.c_str(), err.c_str());
            continue;
        }
        for (int cy = 0; cy + k_chunk <= h; cy += k_chunk)
            for (int cx = 0; cx + k_chunk <= w; cx += k_chunk)
            {
                std::vector<std::uint8_t> c(static_cast<std::size_t>(k_chunk) * k_chunk * 4);
                for (int y = 0; y < k_chunk; ++y)
                    std::memcpy(c.data() + static_cast<std::size_t>(y) * k_chunk * 4,
                                img.data() + (static_cast<std::size_t>(cy + y) * w + cx) * 4, k_chunk * 4);
                std::vector<std::uint8_t> dn, dh;
                const reading n  = measure(c, STB_DXT_NORMAL, dn);
                const reading hq = measure(c, STB_DXT_HIGHQUAL, dh);
                std::string name = f.substr(f.find_last_of("/\\") + 1);
                char at[32];
                std::snprintf(at, sizeof at, "@%d,%d", cx, cy);
                std::printf("%-40s %5s %9.2f %7.2f %4d %9.2f %7.2f %4d\n", name.c_str(), at,
                            n.ms, n.psnr, n.max_err, hq.ms, hq.psnr, hq.max_err);
                ms_n.push_back(n.ms); ms_h.push_back(hq.ms); ps_n.push_back(n.psnr); ps_h.push_back(hq.psnr);
                if (!out_dir.empty())
                {
                    std::vector<std::uint8_t> srgb(static_cast<std::size_t>(k_chunk) * k_chunk * 3);
                    for (std::size_t p = 0; p < srgb.size() / 3; ++p)
                        for (int k = 0; k < 3; ++k)
                            srgb[p * 3 + k] = c[p * 4 + k];
                    const std::string base = out_dir + "/bc1_" + std::to_string(chunk_id);
                    write_png_rgb(base + "_src.png", k_chunk, k_chunk, srgb);
                    write_png_rgb(base + "_bc1.png", k_chunk, k_chunk, dn);
                    write_png_rgb(base + "_bc1hq.png", k_chunk, k_chunk, dh);
                }
                ++chunk_id;
            }
    }
    if (ms_n.empty())
        return 1;
    const auto med = [](std::vector<double> v) {
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    };
    std::printf("MEDIAN over %zu chunks: normal %.2f ms (PSNR %.2f dB), HQ %.2f ms (PSNR %.2f dB)\n",
                ms_n.size(), med(ms_n), med(ps_n), med(ms_h), med(ps_h));
    return 0;
}
