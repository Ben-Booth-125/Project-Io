#include "png_decode.hpp"

#include <cstdlib>
#include <cstring>

namespace ui {

namespace {

// ── Inflate (RFC 1951), after the canonical-Huffman shape of zlib's puff.c ──

struct bit_reader
{
    const std::uint8_t* in  = nullptr;
    std::size_t         len = 0;
    std::size_t         pos = 0;
    std::uint32_t       buf = 0;
    int                 cnt = 0;
    bool                bad = false;

    int bits(int need)
    {
        std::uint32_t v = buf;
        while (cnt < need)
        {
            if (pos >= len) { bad = true; return 0; }
            v |= static_cast<std::uint32_t>(in[pos++]) << cnt;
            cnt += 8;
        }
        buf = v >> need;
        cnt -= need;
        return static_cast<int>(v & ((1u << need) - 1u));
    }
};

constexpr int k_max_bits = 15;

struct huffman
{
    short count[k_max_bits + 1] = {};
    short symbol[288]           = {};
};

/// Decode one symbol, bit by bit (canonical codes, MSB-first within a code).
int decode(bit_reader& s, const huffman& h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= k_max_bits; ++len)
    {
        code |= s.bits(1);
        if (s.bad) return -1;
        const int count = h.count[len];
        if (code - count < first)
            return h.symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1; // ran out of codes
}

/// Build a decoding table from code lengths. Returns < 0 for an over-subscribed set.
int construct(huffman& h, const short* length, int n)
{
    for (int len = 0; len <= k_max_bits; ++len) h.count[len] = 0;
    for (int sym = 0; sym < n; ++sym) h.count[length[sym]]++;
    if (h.count[0] == n) return 0; // no codes: complete, but decoding fails on use
    int left = 1;
    for (int len = 1; len <= k_max_bits; ++len)
    {
        left <<= 1;
        left -= h.count[len];
        if (left < 0) return left;
    }
    short offs[k_max_bits + 1];
    offs[1] = 0;
    for (int len = 1; len < k_max_bits; ++len) offs[len + 1] = static_cast<short>(offs[len] + h.count[len]);
    for (int sym = 0; sym < n; ++sym)
        if (length[sym] != 0) h.symbol[offs[length[sym]]++] = static_cast<short>(sym);
    return left;
}

constexpr short k_len_base[29]  = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                   35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr short k_len_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                   3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr short k_dist_base[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                                   257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                                   8193, 12289, 16385, 24577};
constexpr short k_dist_extra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                                    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

bool codes(bit_reader& s, std::vector<std::uint8_t>& out, const huffman& lencode, const huffman& distcode)
{
    for (;;)
    {
        int sym = decode(s, lencode);
        if (sym < 0) return false;
        if (sym < 256)
            out.push_back(static_cast<std::uint8_t>(sym));
        else if (sym == 256)
            return true;
        else
        {
            sym -= 257;
            if (sym >= 29) return false;
            const int len = k_len_base[sym] + s.bits(k_len_extra[sym]);
            const int ds  = decode(s, distcode);
            if (ds < 0 || ds >= 30) return false;
            const std::size_t dist = static_cast<std::size_t>(k_dist_base[ds] + s.bits(k_dist_extra[ds]));
            if (s.bad || dist > out.size()) return false;
            const std::size_t from = out.size() - dist;
            for (int i = 0; i < len; ++i)
                out.push_back(out[from + static_cast<std::size_t>(i)]);
        }
    }
}

bool inflate_stored(bit_reader& s, std::vector<std::uint8_t>& out)
{
    s.buf = 0;
    s.cnt = 0; // discard to the byte boundary
    if (s.pos + 4 > s.len) return false;
    const unsigned len  = s.in[s.pos] | (s.in[s.pos + 1] << 8);
    const unsigned nlen = s.in[s.pos + 2] | (s.in[s.pos + 3] << 8);
    s.pos += 4;
    if (len != (~nlen & 0xFFFFu) || s.pos + len > s.len) return false;
    out.insert(out.end(), s.in + s.pos, s.in + s.pos + len);
    s.pos += len;
    return true;
}

bool inflate_fixed(bit_reader& s, std::vector<std::uint8_t>& out)
{
    static huffman lencode, distcode;
    static bool    built = false;
    if (!built)
    {
        short lengths[288];
        int   sym = 0;
        for (; sym < 144; ++sym) lengths[sym] = 8;
        for (; sym < 256; ++sym) lengths[sym] = 9;
        for (; sym < 280; ++sym) lengths[sym] = 7;
        for (; sym < 288; ++sym) lengths[sym] = 8;
        construct(lencode, lengths, 288);
        for (sym = 0; sym < 30; ++sym) lengths[sym] = 5;
        construct(distcode, lengths, 30);
        built = true;
    }
    return codes(s, out, lencode, distcode);
}

bool inflate_dynamic(bit_reader& s, std::vector<std::uint8_t>& out)
{
    static constexpr short k_order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    const int nlen  = s.bits(5) + 257;
    const int ndist = s.bits(5) + 1;
    const int ncode = s.bits(4) + 4;
    if (s.bad || nlen > 286 || ndist > 30) return false;

    short lengths[320] = {};
    for (int i = 0; i < ncode; ++i) lengths[k_order[i]] = static_cast<short>(s.bits(3));
    huffman lencode, distcode;
    if (construct(lencode, lengths, 19) != 0) return false;

    int index = 0;
    while (index < nlen + ndist)
    {
        int sym = decode(s, lencode);
        if (sym < 0) return false;
        if (sym < 16)
            lengths[index++] = static_cast<short>(sym);
        else
        {
            short len = 0;
            int   rep = 0;
            if (sym == 16)
            {
                if (index == 0) return false;
                len = lengths[index - 1];
                rep = 3 + s.bits(2);
            }
            else if (sym == 17) rep = 3 + s.bits(3);
            else                rep = 11 + s.bits(7);
            if (s.bad || index + rep > nlen + ndist) return false;
            while (rep--) lengths[index++] = len;
        }
    }
    if (lengths[256] == 0) return false;
    const int lerr = construct(lencode, lengths, nlen);
    if (lerr < 0 || (lerr > 0 && nlen - lencode.count[0] != 1)) return false;
    const int derr = construct(distcode, lengths + nlen, ndist);
    if (derr < 0 || (derr > 0 && ndist - distcode.count[0] != 1)) return false;
    return codes(s, out, lencode, distcode);
}

bool zlib_inflate(const std::vector<std::uint8_t>& z, std::vector<std::uint8_t>& out)
{
    if (z.size() < 2) return false;
    if ((z[0] & 0x0F) != 8 || ((z[0] << 8) | z[1]) % 31 != 0 || (z[1] & 0x20) != 0)
        return false; // deflate, valid check bits, no preset dictionary
    bit_reader s;
    s.in  = z.data() + 2;
    s.len = z.size() - 2;
    int last = 0;
    do
    {
        last = s.bits(1);
        const int type = s.bits(2);
        if (s.bad) return false;
        bool ok = false;
        if      (type == 0) ok = inflate_stored(s, out);
        else if (type == 1) ok = inflate_fixed(s, out);
        else if (type == 2) ok = inflate_dynamic(s, out);
        if (!ok || s.bad) return false;
    } while (!last);
    return true;
}

std::uint32_t be32(const std::uint8_t* p)
{
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}

int paeth(int a, int b, int c)
{
    const int p = a + b - c;
    const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    return pb <= pc ? b : c;
}

} // namespace

bool png_decode_rgba(const std::vector<std::uint8_t>& file, int& width, int& height,
                     std::vector<std::uint8_t>& rgba, std::string& err)
{
    static constexpr std::uint8_t k_sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (file.size() < 8 || std::memcmp(file.data(), k_sig, 8) != 0) { err = "not a PNG"; return false; }

    std::uint32_t w = 0, h = 0;
    int depth = 0, colour = -1, interlace = 0;
    std::vector<std::uint8_t> idat, palette, trns;
    std::size_t p = 8;
    while (p + 12 <= file.size())
    {
        const std::uint32_t len = be32(&file[p]);
        if (p + 12 + len > file.size()) { err = "truncated chunk"; return false; }
        const char* type = reinterpret_cast<const char*>(&file[p + 4]);
        const std::uint8_t* d = &file[p + 8];
        if (std::memcmp(type, "IHDR", 4) == 0 && len >= 13)
        {
            w = be32(d); h = be32(d + 4);
            depth = d[8]; colour = d[9]; interlace = d[12];
        }
        else if (std::memcmp(type, "PLTE", 4) == 0) palette.assign(d, d + len);
        else if (std::memcmp(type, "tRNS", 4) == 0) trns.assign(d, d + len);
        else if (std::memcmp(type, "IDAT", 4) == 0) idat.insert(idat.end(), d, d + len);
        else if (std::memcmp(type, "IEND", 4) == 0) break;
        p += 12 + len;
    }
    if (w == 0 || h == 0 || w > 8192 || h > 8192) { err = "bad size"; return false; }
    if (depth != 8) { err = "only 8-bit channels are supported"; return false; }
    if (interlace != 0) { err = "interlaced PNGs are not supported"; return false; }
    int channels = 0;
    switch (colour)
    {
        case 0: channels = 1; break;
        case 2: channels = 3; break;
        case 3: channels = 1; break;
        case 4: channels = 2; break;
        case 6: channels = 4; break;
        default: err = "unknown colour type"; return false;
    }
    if (colour == 3 && palette.size() < 3) { err = "palette image without PLTE"; return false; }

    std::vector<std::uint8_t> raw;
    raw.reserve(static_cast<std::size_t>(h) * (w * channels + 1));
    if (!zlib_inflate(idat, raw)) { err = "corrupt image data"; return false; }
    const std::size_t stride = static_cast<std::size_t>(w) * channels;
    if (raw.size() < static_cast<std::size_t>(h) * (stride + 1)) { err = "short image data"; return false; }

    // Unfilter in place, row by row.
    std::vector<std::uint8_t> prev(stride, 0), cur(stride);
    rgba.assign(static_cast<std::size_t>(w) * h * 4, 0);
    for (std::uint32_t y = 0; y < h; ++y)
    {
        const std::uint8_t* row = &raw[y * (stride + 1)];
        const int filter = row[0];
        for (std::size_t i = 0; i < stride; ++i)
        {
            const int x = row[1 + i];
            const int a = i >= static_cast<std::size_t>(channels) ? cur[i - channels] : 0;
            const int b = prev[i];
            const int c = i >= static_cast<std::size_t>(channels) ? prev[i - channels] : 0;
            int v = x;
            switch (filter)
            {
                case 0: break;
                case 1: v = x + a; break;
                case 2: v = x + b; break;
                case 3: v = x + ((a + b) >> 1); break;
                case 4: v = x + paeth(a, b, c); break;
                default: err = "unknown filter"; return false;
            }
            cur[i] = static_cast<std::uint8_t>(v & 0xFF);
        }
        std::uint8_t* dst = &rgba[static_cast<std::size_t>(y) * w * 4];
        for (std::uint32_t x = 0; x < w; ++x)
        {
            const std::uint8_t* s = &cur[x * channels];
            std::uint8_t r = 0, g = 0, bl = 0, al = 255;
            switch (colour)
            {
                case 0: r = g = bl = s[0]; break;
                case 2: r = s[0]; g = s[1]; bl = s[2]; break;
                case 3:
                {
                    const std::size_t k = static_cast<std::size_t>(s[0]) * 3;
                    if (k + 2 < palette.size()) { r = palette[k]; g = palette[k + 1]; bl = palette[k + 2]; }
                    if (s[0] < trns.size()) al = trns[s[0]];
                    break;
                }
                case 4: r = g = bl = s[0]; al = s[1]; break;
                case 6: r = s[0]; g = s[1]; bl = s[2]; al = s[3]; break;
            }
            dst[x * 4 + 0] = r; dst[x * 4 + 1] = g; dst[x * 4 + 2] = bl; dst[x * 4 + 3] = al;
        }
        std::swap(prev, cur);
    }
    width  = static_cast<int>(w);
    height = static_cast<int>(h);
    return true;
}

} // namespace ui
