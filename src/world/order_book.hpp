#pragma once

#include "world.hpp"

#include <cstdint>
#include <iosfwd>

// ---------------------------------------------------------------------------
// Order book — serialisation (BL-293)
// ---------------------------------------------------------------------------
// world.hpp declares the shape (`world::sell_orders`, `world::buy_orders`,
// `world::next_order_id`); components.hpp declares the records. This header is
// the order book's leg of the flat-binary serialisation seam, and the SECOND
// such stream in world/* after history_log.{hpp,cpp} — it follows that file
// exactly: leading magic + version (docs/tech/TECH_FOUNDATIONS.md's "the first
// thing to add when the serialiser lands", BL-107), count-prefixed records, and
// rejection rather than reinterpretation on anything that does not parse.
//
// Why the order book needed one at all: it lived on `ui_state` until 2026-08-07,
// so a player's standing orders were never save-format state and would not have
// survived a save/load — a defect independent of the word interface that shipped
// BL-293. Putting the book in `world` fixes reachability; putting it on this seam
// fixes persistence, and the two are the same move.
//
// Deliberately standalone, for history_log.hpp's reason: the command seam
// (corp_command.cpp) and the clearing tick (market_clearing.cpp) touch the book
// through world.hpp alone, so neither they nor the dozen hand-written
// tools/verify recipes that link them need this translation unit.
// ---------------------------------------------------------------------------

/// Leading magic identifying an order-book stream: the bytes 'I','O','O','B'.
/// read_order_book rejects any stream that does not start with this exactly,
/// rather than guessing at a layout.
inline constexpr uint32_t order_book_magic =
    (uint32_t('I')) | (uint32_t('O') << 8) | (uint32_t('O') << 16) | (uint32_t('B') << 24);

/// Format version. Bump on any layout change to `sell_order` / `buy_order` or to
/// the stream shape — the `static_assert`s on both record sizes (components.hpp)
/// are the tripwire that says when. read_order_book rejects a mismatched version
/// rather than reinterpreting a differently-shaped record.
inline constexpr uint32_t order_book_version = 1;

/// Sanity ceiling on each side's declared order count. Guards read_order_book
/// against an eager large allocation from a corrupt or maliciously-crafted count
/// before the per-record reads get a chance to fail on a truncated stream. Far
/// above any real book — the command seam caps a corp at
/// `max_sell_orders_per_corp` (corp_command.hpp) and there are a dozen corps.
inline constexpr uint32_t order_book_max_orders = 1u << 20; // ~1M orders per side

/// Write @p w's order book as: a 4-byte magic + a uint32_t version header, then
/// the uint32_t `next_order_id`, then each side as a uint32_t count followed by
/// its records (sell: id, corp, body, resource, quantity, floor_price; buy: id,
/// corp, body, resource, quantity, max_price, preferred_seller). Binary, native
/// byte order — the project's settled flat-binary convention (no other world/*
/// stream is byte-order-normalised either).
///
/// STORED ORDER IS PRESERVED, and that is load-bearing rather than incidental:
/// matching is price-TIME priority, so the sequence of the book is part of the
/// state. A round-trip that reordered the book would clear differently.
///
/// @param w   World whose order book is serialised.
/// @param out Binary output stream, positioned where the stream should start.
void write_order_book(const world& w, std::ostream& out);

/// Read a stream written by write_order_book back into @p w, REPLACING its
/// `sell_orders`, `buy_orders` and `next_order_id` on success. Rejects (returns
/// false, leaving all three untouched) rather than proceeding to interpret
/// garbage bytes as orders when: the magic does not match, the version is not
/// `order_book_version`, the stream is short/truncated, a resource byte does not
/// name a known `resource_type`, or a declared count exceeds
/// `order_book_max_orders`.
///
/// @param w  World whose order book is replaced on success.
/// @param in Binary input stream, positioned where the stream should start.
/// @return   True on a fully-read, well-formed stream; false otherwise.
bool read_order_book(world& w, std::istream& in);
