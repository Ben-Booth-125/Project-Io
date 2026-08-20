#include "procurement.hpp"

#include <istream>
#include <map>
#include <ostream>
#include <utility>
#include <vector>

namespace {

// Deliberately a local copy of order_book.cpp's binary primitives — see that
// file's own comment on why this is not hoisted into a shared header yet.

void write_u8(std::ostream& out, uint8_t v)
{
    out.write(reinterpret_cast<const char*>(&v), sizeof v);
}

void write_u32(std::ostream& out, uint32_t v)
{
    out.write(reinterpret_cast<const char*>(&v), sizeof v);
}

void write_i32(std::ostream& out, int32_t v)
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

bool read_i32(std::istream& in, int32_t& v)
{
    in.read(reinterpret_cast<char*>(&v), sizeof v);
    return static_cast<bool>(in);
}

bool read_f32(std::istream& in, float& v)
{
    in.read(reinterpret_cast<char*>(&v), sizeof v);
    return static_cast<bool>(in);
}

bool valid_resource_id(uint8_t id)
{
    return id < static_cast<uint8_t>(resource_type::count);
}

} // namespace

void write_procurement(const world& w, std::ostream& out)
{
    write_u32(out, procurement_magic);
    write_u32(out, procurement_version);
    write_u32(out, w.next_procurement_id);

    write_u32(out, static_cast<uint32_t>(w.procurement_quotes.size()));
    for (const procurement_quote& q : w.procurement_quotes)
    {
        write_u32(out, q.id);
        write_u32(out, q.buyer);
        write_u32(out, q.supplier);
        write_u32(out, q.body);
        write_u32(out, q.delivery_body);
        write_u8(out, static_cast<uint8_t>(q.resource));
        write_f32(out, q.quantity);
        write_f32(out, q.unit_price);
        write_i32(out, q.lead_time_ticks);
        write_f32(out, q.freight_cost);
    }

    write_u32(out, static_cast<uint32_t>(w.procurement_contracts.size()));
    for (const procurement_contract& c : w.procurement_contracts)
    {
        write_u32(out, c.id);
        write_u32(out, c.buyer);
        write_u32(out, c.supplier);
        write_u32(out, c.body);
        write_u32(out, c.delivery_body);
        write_u8(out, static_cast<uint8_t>(c.resource));
        write_f32(out, c.quantity);
        write_f32(out, c.unit_price);
        write_i32(out, c.lead_time_ticks);
        write_i32(out, c.ticks_elapsed);
        write_f32(out, c.deposit_paid);
        write_f32(out, c.freight_cost);
    }

    write_u32(out, static_cast<uint32_t>(w.corp_reputation.size()));
    for (const auto& [pair, value] : w.corp_reputation)
    {
        write_u32(out, pair.first);
        write_u32(out, pair.second);
        write_f32(out, value);
    }
}

bool read_procurement(world& w, std::istream& in)
{
    uint32_t magic = 0;
    if (!read_u32(in, magic) || magic != procurement_magic)
        return false;

    uint32_t version = 0;
    if (!read_u32(in, version) || version != procurement_version)
        return false;

    uint32_t next_id = 0;
    if (!read_u32(in, next_id))
        return false;

    uint32_t quote_count = 0;
    if (!read_u32(in, quote_count) || quote_count > procurement_max_records)
        return false;

    std::vector<procurement_quote> quotes;
    quotes.reserve(quote_count);
    for (uint32_t i = 0; i < quote_count; ++i)
    {
        procurement_quote q;
        uint8_t           res = 0;
        if (!read_u32(in, q.id) || !read_u32(in, q.buyer) || !read_u32(in, q.supplier) ||
            !read_u32(in, q.body) || !read_u32(in, q.delivery_body))
            return false;
        if (!read_u8(in, res) || !valid_resource_id(res))
            return false;
        q.resource = static_cast<resource_type>(res);
        if (!read_f32(in, q.quantity) || !read_f32(in, q.unit_price) ||
            !read_i32(in, q.lead_time_ticks) || !read_f32(in, q.freight_cost))
            return false;
        quotes.push_back(q);
    }

    uint32_t contract_count = 0;
    if (!read_u32(in, contract_count) || contract_count > procurement_max_records)
        return false;

    std::vector<procurement_contract> contracts;
    contracts.reserve(contract_count);
    for (uint32_t i = 0; i < contract_count; ++i)
    {
        procurement_contract c;
        uint8_t              res = 0;
        if (!read_u32(in, c.id) || !read_u32(in, c.buyer) || !read_u32(in, c.supplier) ||
            !read_u32(in, c.body) || !read_u32(in, c.delivery_body))
            return false;
        if (!read_u8(in, res) || !valid_resource_id(res))
            return false;
        c.resource = static_cast<resource_type>(res);
        if (!read_f32(in, c.quantity) || !read_f32(in, c.unit_price) ||
            !read_i32(in, c.lead_time_ticks) || !read_i32(in, c.ticks_elapsed) ||
            !read_f32(in, c.deposit_paid) || !read_f32(in, c.freight_cost))
            return false;
        contracts.push_back(c);
    }

    uint32_t rep_count = 0;
    if (!read_u32(in, rep_count) || rep_count > procurement_max_records)
        return false;

    std::map<std::pair<entity_id, entity_id>, float> reputation;
    for (uint32_t i = 0; i < rep_count; ++i)
    {
        entity_id buyer = null_entity, supplier = null_entity;
        float     value = 0.0f;
        if (!read_u32(in, buyer) || !read_u32(in, supplier) || !read_f32(in, value))
            return false;
        reputation[{buyer, supplier}] = value;
    }

    // Replace only on full success — a truncated stream leaves the destination
    // world's procurement state exactly as it was (read_order_book's contract).
    w.procurement_quotes    = std::move(quotes);
    w.procurement_contracts = std::move(contracts);
    w.next_procurement_id   = next_id;
    w.corp_reputation       = std::move(reputation);
    return true;
}
