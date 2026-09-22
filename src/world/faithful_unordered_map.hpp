#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

/// A `std::unordered_map` whose COPY iterates in its SOURCE'S order (BL-1034).
///
/// WHY THIS EXISTS. A world copy must tick byte for byte as its original, and a
/// tick reads some of `world`'s unordered stores in iteration order where the
/// order reaches the arithmetic — `body_mean_habitability` (budget_system.cpp)
/// sums every centre's habitability in `population_centres` order, and float
/// addition is not associative. So a store's iteration order is part of what a
/// tick computes, and a copy has to carry it.
///
/// The standard promises nothing about a copy's order, and MSVC's copy does not
/// keep it: the copy constructor takes the source's bucket count and re-inserts
/// the elements one at a time in iteration order, and MSVC inserts a new key at
/// the FRONT of its bucket. Every bucket holding two or more keys therefore comes
/// out REVERSED. Measured on library seed 28 (2026-09-18,
/// tools/verify/world_copy_determinism.cpp): 13 of world's 18 unordered stores
/// iterated differently in a copy; one body's mean habitability moved two ULP
/// (0.43268615 -> 0.43268609), the wages built on it moved 31 of 87 balances in
/// the first tick's budget lap, and the validation settle ended on a different
/// world (D_settle 18F78EB9B2B20F29, the original's pin 265C48A23E313B1A).
///
/// HOW IT COPIES. As the library copies. If that copy already iterates as its
/// source (a library whose copy keeps order, or a map with no shared bucket) it
/// is kept; otherwise it is copied once more — reversing every bucket twice is
/// the identity. Either way the result is CHECKED key for key, with the bucket
/// count and the load factor, and a copy that still would not iterate as its
/// source throws rather than hand back a world that silently ticks differently.
///
/// EVERYTHING ELSE IS `std::unordered_map`, UNCHANGED. Construction, lookup,
/// insertion, erasure, rehash and MOVES (which hand the node list over whole)
/// behave exactly as before, so a world that is never copied runs the same
/// bytes it always did. Only a copy of this type is faithful: copying out into a
/// plain `std::unordered_map` gets the library's copy, as it always did.
template <class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>,
          class Alloc = std::allocator<std::pair<const Key, T>>>
class faithful_unordered_map : public std::unordered_map<Key, T, Hash, KeyEqual, Alloc>
{
public:
    using base = std::unordered_map<Key, T, Hash, KeyEqual, Alloc>;
    using base::base;

    faithful_unordered_map() = default;
    faithful_unordered_map(faithful_unordered_map&&) = default;
    faithful_unordered_map& operator=(faithful_unordered_map&&) = default;
    ~faithful_unordered_map() = default;

    faithful_unordered_map(const faithful_unordered_map& src)
        : base(static_cast<const base&>(src))
    {
        if (!iterates_as(src))
        {
            base again(static_cast<const base&>(*this));
            base::swap(again);
        }
        if (!iterates_as(src))
            throw std::logic_error(
                "faithful_unordered_map: a copy does not iterate in its source's order");
    }

    faithful_unordered_map& operator=(const faithful_unordered_map& src)
    {
        if (this != &src)
        {
            faithful_unordered_map copy(src);
            base::swap(copy);   // a swap exchanges the node lists whole: order kept
        }
        return *this;
    }

    /// True when this map walks the same keys in the same order as @p other,
    /// over the same bucket count and maximum load factor — everything a later
    /// insertion or rehash reads, so two maps that pass it also stay in step.
    bool iterates_as(const base& other) const
    {
        if (this->size() != other.size() || this->bucket_count() != other.bucket_count()
            || this->max_load_factor() != other.max_load_factor())
            return false;
        const KeyEqual eq = this->key_eq();
        auto it = other.begin();
        for (const auto& kv : *this)
        {
            if (!eq(kv.first, it->first))
                return false;
            ++it;
        }
        return true;
    }
};
