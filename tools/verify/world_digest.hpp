// ---------------------------------------------------------------------------
// The BL-1031 world digest, shared (BL-1034)
// ---------------------------------------------------------------------------
// FNV-1a 64 over RAW bytes, floats and doubles included, so a digest moves on a
// last-bit change a printed `%.4f` would hide. Records are hashed FIELD BY FIELD,
// never as a block, so struct padding cannot enter a digest (and `scalar`
// refuses anything that is not a number or an enum).
//
// Lifted out of player_seed_sweep.cpp unchanged, so the instrument that asks
// "does a copied world tick as its original" (world_copy_determinism) hashes a
// world exactly as the BL-1031 pins do, rather than inventing a second digest
// that could agree with itself and disagree with the pins.
#pragma once

#include "world/world.hpp"
#include "world/world_save.hpp"

#include <cstddef>
#include <cstdint>
#include <ios>
#include <sstream>
#include <string>
#include <type_traits>

struct fnv1a64
{
    std::uint64_t h = 0xCBF29CE484222325ull;

    void bytes(const void* p, std::size_t n)
    {
        const auto* b = static_cast<const unsigned char*>(p);
        for (std::size_t i = 0; i < n; ++i)
        {
            h ^= b[i];
            h *= 0x00000100000001B3ull;
        }
    }
    template <class T>
    void scalar(T v)
    {
        static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>,
                      "hash a record field by field, never as a block");
        bytes(&v, sizeof v);
    }
    void flag(bool v) { scalar(static_cast<std::uint8_t>(v ? 1u : 0u)); }
    void count(std::size_t n) { scalar(static_cast<std::uint64_t>(n)); }
};

/// The world's flat-binary save bytes, into @p f. Returns the byte count, so a
/// digest row can show it hashed megabytes rather than an empty stream.
inline std::size_t hash_snapshot(fnv1a64& f, const world& w)
{
    std::ostringstream os(std::ios::out | std::ios::binary);
    write_world_snapshot(w, os);
    const std::string bytes = os.str();
    f.bytes(bytes.data(), bytes.size());
    return bytes.size();
}

/// BL-1031's D_settle recipe: the snapshot bytes, then `state_hash` at
/// `current_day_tick` (the tick the app's verify API hashes at). The pinned
/// digest takes it after the validation run; world_copy_determinism takes it
/// after every tick (and every lap) of that run.
inline std::uint64_t world_state_digest(const world& w, std::size_t* snapshot_bytes = nullptr)
{
    fnv1a64 f;
    const std::size_t n = hash_snapshot(f, w);
    f.scalar(w.state_hash(w.current_day_tick));
    if (snapshot_bytes != nullptr)
        *snapshot_bytes = n;
    return f.h;
}
