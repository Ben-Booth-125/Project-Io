#pragma once

#include "entity.hpp"

#include <array>
#include <cstdint>
#include <istream>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Flat-binary primitives (BL-536)
// ---------------------------------------------------------------------------
// The read/write vocabulary the world snapshot is written in. Little more than
// named `read`/`write` calls, but named ONCE: every field of every serialised
// struct goes through these, so the stream's byte-level conventions (a uint32
// length prefix ahead of every string and every container, a uint8 per enum, a
// hard ceiling ahead of every allocation) live in one file rather than being
// re-decided per struct.
//
// WHY FIELD-WISE AND NOT A RAW STRUCT WRITE. TECH_FOUNDATIONS says tile structs
// "will be written and read as raw binary", and raw would be faster for the
// ~50k tiles that dominate a snapshot's ~17 MB. Ben chose field-wise anyway
// (2026-08-22): the schema still moves weekly, and a padding-baked layout turns
// every added field into a silent misread that only a remembered version bump
// would catch. Padding, member order and ABI stop mattering here.
//
// THE TWO SHIPPED STREAMS PREDATE THIS HEADER and keep their own private copies
// of these helpers -- `history_log.cpp` (IOHL) and `province.cpp`, whose bytes
// are identical but whose functions are file-local. They are deliberately NOT
// refactored onto this header: their bytes are shipped, a save written by an
// older build must keep reading, and the churn buys nothing a reader can see.
// A new stream should use THIS file. (Flagged to Ben -- NEEDS_REVIEW, 2026-08-22.)
//
// EVERY READ RETURNS bool. False means a short, truncated or out-of-range read;
// callers propagate it as a WHOLE-STREAM rejection rather than keeping the
// partial data. That is BL-107's rule ("reject rather than reinterpret") applied
// one field at a time.
// ---------------------------------------------------------------------------

namespace bio
{

/// Ceiling on any single length-prefixed string. Guards a read against sizing an
/// enormous allocation from a corrupt or hostile length prefix before the field
/// read gets a chance to fail on a short stream. No real field approaches this.
inline constexpr uint32_t max_field_bytes = 1u << 20; // 1 MiB

/// Ceiling on any single container's declared element count. Same guard, same
/// reason: a claimed count near UINT32_MAX would otherwise reserve hundreds of
/// gigabytes before the first element read could fail. A homeworld's tile map --
/// the largest container in a snapshot -- is ~31.5k entries.
inline constexpr uint32_t max_count = 1u << 26; // ~67M elements

// ---------------------------------------------------------------------------
// Write side
// ---------------------------------------------------------------------------

/// Write one trivially-copyable scalar in host byte order.
///
/// Host order rather than a fixed endianness is deliberate and matches the three
/// shipped streams: a prototype save is not a wire format and never crosses
/// architectures. If that changes, this is the one function to change.
///
/// @param out Stream to append to.
/// @param v   Value to write.
template <class T>
inline void w_pod(std::ostream& out, const T& v)
{
    static_assert(std::is_trivially_copyable_v<T>, "w_pod needs a trivially-copyable type");
    out.write(reinterpret_cast<const char*>(&v), sizeof v);
}

inline void w_u8(std::ostream& out, uint8_t v) { w_pod(out, v); }
inline void w_u16(std::ostream& out, uint16_t v) { w_pod(out, v); }
inline void w_u32(std::ostream& out, uint32_t v) { w_pod(out, v); }
inline void w_u64(std::ostream& out, uint64_t v) { w_pod(out, v); }
inline void w_i32(std::ostream& out, int32_t v) { w_pod(out, v); }
inline void w_i64(std::ostream& out, int64_t v) { w_pod(out, v); }
inline void w_f32(std::ostream& out, float v) { w_pod(out, v); }
inline void w_f64(std::ostream& out, double v) { w_pod(out, v); }

/// Write a bool as one byte (0/1) rather than as `sizeof(bool)`, which is
/// implementation-defined.
inline void w_bool(std::ostream& out, bool v) { w_u8(out, v ? 1u : 0u); }

/// Write an `int` as a fixed 32-bit field. `int` is 32-bit on every target the
/// project builds for, but the stream should not depend on that staying true.
inline void w_int(std::ostream& out, int v) { w_i32(out, static_cast<int32_t>(v)); }

/// Write a `std::size_t` as a 32-bit count. Every count in a snapshot is bounded
/// by `max_count`, well inside 32 bits.
inline void w_count(std::ostream& out, std::size_t n) { w_u32(out, static_cast<uint32_t>(n)); }

inline void w_id(std::ostream& out, entity_id v) { w_u32(out, v); }

/// Write a uint8-backed enum as its one underlying byte. The read side range-checks
/// against the enum's own maximum -- see `r_enum`.
template <class E>
inline void w_enum(std::ostream& out, E v)
{
    static_assert(std::is_enum_v<E>, "w_enum needs an enum type");
    w_u8(out, static_cast<uint8_t>(v));
}

/// Write a uint32 length prefix followed by the raw bytes.
inline void w_str(std::ostream& out, const std::string& s)
{
    w_count(out, s.size());
    if (!s.empty())
        out.write(s.data(), static_cast<std::streamsize>(s.size()));
}

/// Write a fixed-size float array as its elements, with NO count -- the length is
/// part of the schema (`resource_count`), not part of the data. A change to
/// `resource_count` is therefore a format change needing a version bump, which is
/// the honest reading: the array's meaning changed, not just its size.
template <std::size_t N>
inline void w_f32_array(std::ostream& out, const std::array<float, N>& a)
{
    for (float v : a)
        w_f32(out, v);
}

/// Write a fixed-size bool array (one byte per element), same no-count rule.
template <std::size_t N>
inline void w_bool_array(std::ostream& out, const std::array<bool, N>& a)
{
    for (bool v : a)
        w_bool(out, v);
}

/// Write a count-prefixed vector of entity ids.
inline void w_ids(std::ostream& out, const std::vector<entity_id>& v)
{
    w_count(out, v.size());
    for (const entity_id id : v)
        w_id(out, id);
}

/// Write a count-prefixed vector of ints.
inline void w_ints(std::ostream& out, const std::vector<int>& v)
{
    w_count(out, v.size());
    for (const int x : v)
        w_int(out, x);
}

/// Write a count-prefixed vector of floats.
inline void w_floats(std::ostream& out, const std::vector<float>& v)
{
    w_count(out, v.size());
    for (const float x : v)
        w_f32(out, x);
}

/// Write a count-prefixed vector of bytes.
inline void w_bytes(std::ostream& out, const std::vector<uint8_t>& v)
{
    w_count(out, v.size());
    for (const uint8_t x : v)
        w_u8(out, x);
}

/// Write a count-prefixed vector of elements, each through @p write_one.
///
/// @param out       Stream to append to.
/// @param v         Elements to write.
/// @param write_one Per-element writer, `(std::ostream&, const T&)`.
template <class T, class F>
inline void w_vec(std::ostream& out, const std::vector<T>& v, F write_one)
{
    w_count(out, v.size());
    for (const T& e : v)
        write_one(out, e);
}

/// Write a count-prefixed `std::set` of id pairs, in the set's own (sorted)
/// order -- which is why the stance tables round-trip without a re-sort.
inline void w_id_pair_set(std::ostream& out, const std::set<std::pair<entity_id, entity_id>>& s)
{
    w_count(out, s.size());
    for (const auto& pr : s)
    {
        w_id(out, pr.first);
        w_id(out, pr.second);
    }
}

/// Write a count-prefixed ordered map, key then value, in the map's own sorted
/// order. Deterministic by construction -- the reason every pair-keyed table in
/// `world` is a `std::map` and not an `unordered_map`.
template <class K, class V, class FK, class FV>
inline void w_map(std::ostream& out, const std::map<K, V>& m, FK write_key, FV write_val)
{
    w_count(out, m.size());
    for (const auto& kv : m)
    {
        write_key(out, kv.first);
        write_val(out, kv.second);
    }
}

// ---------------------------------------------------------------------------
// Read side -- every function returns false on a short or invalid read
// ---------------------------------------------------------------------------

/// Read one trivially-copyable scalar in host byte order.
///
/// @param in Stream to read from.
/// @param v  Destination.
/// @return   False on a short or failed read; @p v is then unspecified.
template <class T>
inline bool r_pod(std::istream& in, T& v)
{
    static_assert(std::is_trivially_copyable_v<T>, "r_pod needs a trivially-copyable type");
    in.read(reinterpret_cast<char*>(&v), sizeof v);
    return static_cast<bool>(in);
}

inline bool r_u8(std::istream& in, uint8_t& v) { return r_pod(in, v); }
inline bool r_u16(std::istream& in, uint16_t& v) { return r_pod(in, v); }
inline bool r_u32(std::istream& in, uint32_t& v) { return r_pod(in, v); }
inline bool r_u64(std::istream& in, uint64_t& v) { return r_pod(in, v); }
inline bool r_i32(std::istream& in, int32_t& v) { return r_pod(in, v); }
inline bool r_i64(std::istream& in, int64_t& v) { return r_pod(in, v); }
inline bool r_f32(std::istream& in, float& v) { return r_pod(in, v); }
inline bool r_f64(std::istream& in, double& v) { return r_pod(in, v); }

/// Read a one-byte bool. ANY non-zero byte reads as true rather than only 1 --
/// the write side emits 0/1, so a byte outside that is corruption the later
/// fields will catch, and rejecting here would add a failure mode without
/// adding a guarantee.
inline bool r_bool(std::istream& in, bool& v)
{
    uint8_t b = 0;
    if (!r_u8(in, b))
        return false;
    v = (b != 0);
    return true;
}

inline bool r_int(std::istream& in, int& v)
{
    int32_t x = 0;
    if (!r_i32(in, x))
        return false;
    v = static_cast<int>(x);
    return true;
}

inline bool r_id(std::istream& in, entity_id& v) { return r_u32(in, v); }

/// Read a container's element count, rejecting anything past `max_count` BEFORE
/// a caller sizes an allocation against it.
inline bool r_count(std::istream& in, uint32_t& n)
{
    if (!r_u32(in, n))
        return false;
    return n <= max_count;
}

/// Read a uint8-backed enum, rejecting any byte past @p max_value.
///
/// The range check is the point: casting an unchecked byte into an enum is
/// exactly the "reinterpret a stale layout as garbage" failure BL-107 exists to
/// prevent, one field down. Pass the enum's largest valid enumerator.
///
/// @param in        Stream to read from.
/// @param v         Destination.
/// @param max_value Largest valid enumerator, inclusive.
/// @return          False on a short read or an out-of-range byte.
template <class E>
inline bool r_enum(std::istream& in, E& v, E max_value)
{
    static_assert(std::is_enum_v<E>, "r_enum needs an enum type");
    uint8_t b = 0;
    if (!r_u8(in, b))
        return false;
    if (b > static_cast<uint8_t>(max_value))
        return false;
    v = static_cast<E>(b);
    return true;
}

/// Read a uint32-length-prefixed string, rejecting a prefix past `max_field_bytes`.
inline bool r_str(std::istream& in, std::string& s)
{
    uint32_t len = 0;
    if (!r_u32(in, len))
        return false;
    if (len > max_field_bytes)
        return false; // corrupt or hostile length prefix -- refuse before allocating
    s.resize(len);
    if (len > 0)
    {
        in.read(s.data(), static_cast<std::streamsize>(len));
        if (!in)
            return false;
    }
    return true;
}

/// Read a fixed-size float array -- no count, matching `w_f32_array`.
template <std::size_t N>
inline bool r_f32_array(std::istream& in, std::array<float, N>& a)
{
    for (std::size_t i = 0; i < N; ++i)
        if (!r_f32(in, a[i]))
            return false;
    return true;
}

/// Read a fixed-size bool array -- no count, matching `w_bool_array`.
template <std::size_t N>
inline bool r_bool_array(std::istream& in, std::array<bool, N>& a)
{
    for (std::size_t i = 0; i < N; ++i)
    {
        bool b = false;
        if (!r_bool(in, b))
            return false;
        a[i] = b;
    }
    return true;
}

inline bool r_ids(std::istream& in, std::vector<entity_id>& v)
{
    uint32_t n = 0;
    if (!r_count(in, n))
        return false;
    v.assign(n, null_entity);
    for (uint32_t i = 0; i < n; ++i)
        if (!r_id(in, v[i]))
            return false;
    return true;
}

inline bool r_ints(std::istream& in, std::vector<int>& v)
{
    uint32_t n = 0;
    if (!r_count(in, n))
        return false;
    v.assign(n, 0);
    for (uint32_t i = 0; i < n; ++i)
        if (!r_int(in, v[i]))
            return false;
    return true;
}

inline bool r_floats(std::istream& in, std::vector<float>& v)
{
    uint32_t n = 0;
    if (!r_count(in, n))
        return false;
    v.assign(n, 0.0f);
    for (uint32_t i = 0; i < n; ++i)
        if (!r_f32(in, v[i]))
            return false;
    return true;
}

inline bool r_bytes(std::istream& in, std::vector<uint8_t>& v)
{
    uint32_t n = 0;
    if (!r_count(in, n))
        return false;
    v.assign(n, 0u);
    for (uint32_t i = 0; i < n; ++i)
        if (!r_u8(in, v[i]))
            return false;
    return true;
}

/// Read a count-prefixed vector, each element through @p read_one.
///
/// Elements are default-constructed and filled in place rather than reserved --
/// `r_count` has already bounded @p n, so the resize is safe, and it keeps the
/// per-element reader a plain `(std::istream&, T&) -> bool`.
///
/// @param in       Stream to read from.
/// @param v        Destination, replaced wholesale.
/// @param read_one Per-element reader, `(std::istream&, T&) -> bool`.
/// @return         False if the count or any element read fails.
template <class T, class F>
inline bool r_vec(std::istream& in, std::vector<T>& v, F read_one)
{
    uint32_t n = 0;
    if (!r_count(in, n))
        return false;
    v.clear();
    v.resize(n);
    for (uint32_t i = 0; i < n; ++i)
        if (!read_one(in, v[i]))
            return false;
    return true;
}

inline bool r_id_pair_set(std::istream& in, std::set<std::pair<entity_id, entity_id>>& s)
{
    uint32_t n = 0;
    if (!r_count(in, n))
        return false;
    s.clear();
    for (uint32_t i = 0; i < n; ++i)
    {
        entity_id a = null_entity, b = null_entity;
        if (!r_id(in, a) || !r_id(in, b))
            return false;
        s.emplace(a, b);
    }
    return true;
}

/// Read a count-prefixed ordered map. The map re-sorts on insert, so a stream
/// whose entries are out of order still loads to the same map -- the sorted write
/// order is a determinism property of the WRITE, not an invariant validated here.
template <class K, class V, class FK, class FV>
inline bool r_map(std::istream& in, std::map<K, V>& m, FK read_key, FV read_val)
{
    uint32_t n = 0;
    if (!r_count(in, n))
        return false;
    m.clear();
    for (uint32_t i = 0; i < n; ++i)
    {
        K k{};
        V v{};
        if (!read_key(in, k) || !read_val(in, v))
            return false;
        m.emplace(std::move(k), std::move(v));
    }
    return true;
}

} // namespace bio
