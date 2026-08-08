#include "order_book.hpp"

#include <istream>
#include <ostream>
#include <utility>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Binary primitives. Deliberately a local copy of history_log.cpp's set rather
// than a shared header: they are four one-line functions, and the alternative
// (a `binary_io.hpp` every stream includes) would couple the two seams together
// for no gain while the serialiser is still being built out one stream at a
// time. If a third stream lands, hoist them then.
// ---------------------------------------------------------------------------

void write_u8(std::ostream& out, uint8_t v)
{
    out.write(reinterpret_cast<const char*>(&v), sizeof v);
}

void write_u32(std::ostream& out, uint32_t v)
{
    out.write(reinterpret_cast<const char*>(&v), sizeof v);
}

void write_f32(std::ostream& out, float v)
{
    out.write(reinterpret_cast<const char*>(&v), sizeof v);
}

bool read_u8(std::istream& in, uint8_t& v)
{
    in.read(reinterpret_cast<char*>(&v), sizeof v);
    return static_cast<bool>(in);
}

bool read_u32(std::istream& in, uint32_t& v)
{
    in.read(reinterpret_cast<char*>(&v), sizeof v);
    return static_cast<bool>(in);
}

bool read_f32(std::istream& in, float& v)
{
    in.read(reinterpret_cast<char*>(&v), sizeof v);
    return static_cast<bool>(in);
}

/// True iff @p id names a real `resource_type`. A byte outside the enum is a
/// corrupt record or one written by a build with more resources than this one
/// knows — either way it is refused, not clamped, so a stale save cannot silently
/// turn one good into another.
bool valid_resource_id(uint8_t id)
{
    return id < static_cast<uint8_t>(resource_type::count);
}

} // namespace

void write_order_book(const world& w, std::ostream& out)
{
    write_u32(out, order_book_magic);
    write_u32(out, order_book_version);
    write_u32(out, w.next_order_id);

    write_u32(out, static_cast<uint32_t>(w.sell_orders.size()));
    for (const sell_order& o : w.sell_orders)
    {
        write_u32(out, o.id);
        write_u32(out, o.corp);
        write_u32(out, o.body);
        write_u8(out, static_cast<uint8_t>(o.resource));
        write_f32(out, o.quantity);
        write_f32(out, o.floor_price);
    }

    write_u32(out, static_cast<uint32_t>(w.buy_orders.size()));
    for (const buy_order& o : w.buy_orders)
    {
        write_u32(out, o.id);
        write_u32(out, o.corp);
        write_u32(out, o.body);
        write_u8(out, static_cast<uint8_t>(o.resource));
        write_f32(out, o.quantity);
        write_f32(out, o.max_price);
        write_u32(out, o.preferred_seller);
    }
}

bool read_order_book(world& w, std::istream& in)
{
    uint32_t magic = 0;
    if (!read_u32(in, magic) || magic != order_book_magic)
        return false; // wrong magic — not an order-book stream at all

    uint32_t version = 0;
    if (!read_u32(in, version) || version != order_book_version)
        return false; // unsupported/mismatched version — refuse rather than misread

    uint32_t next_id = 0;
    if (!read_u32(in, next_id))
        return false;

    uint32_t sell_count = 0;
    if (!read_u32(in, sell_count))
        return false;
    if (sell_count > order_book_max_orders)
        return false; // corrupt/malicious count — refuse before the eager reserve

    std::vector<sell_order> sells;
    sells.reserve(sell_count);
    for (uint32_t i = 0; i < sell_count; ++i)
    {
        sell_order o;
        uint8_t    res = 0;
        if (!read_u32(in, o.id) || !read_u32(in, o.corp) || !read_u32(in, o.body))
            return false;
        if (!read_u8(in, res) || !valid_resource_id(res))
            return false;
        o.resource = static_cast<resource_type>(res);
        if (!read_f32(in, o.quantity) || !read_f32(in, o.floor_price))
            return false;
        sells.push_back(o);
    }

    uint32_t buy_count = 0;
    if (!read_u32(in, buy_count))
        return false;
    if (buy_count > order_book_max_orders)
        return false;

    std::vector<buy_order> buys;
    buys.reserve(buy_count);
    for (uint32_t i = 0; i < buy_count; ++i)
    {
        buy_order o;
        uint8_t   res = 0;
        if (!read_u32(in, o.id) || !read_u32(in, o.corp) || !read_u32(in, o.body))
            return false;
        if (!read_u8(in, res) || !valid_resource_id(res))
            return false;
        o.resource = static_cast<resource_type>(res);
        if (!read_f32(in, o.quantity) || !read_f32(in, o.max_price) ||
            !read_u32(in, o.preferred_seller))
            return false;
        buys.push_back(o);
    }

    // Replace only on full success — a truncated stream leaves the destination
    // world's book exactly as it was (read_history_log's contract).
    w.sell_orders   = std::move(sells);
    w.buy_orders    = std::move(buys);
    w.next_order_id = next_id;
    return true;
}
