#pragma once

#include "world.hpp"

#include <cstdint>
#include <iosfwd>

// ---------------------------------------------------------------------------
// Procurement — serialisation (BL-350)
// ---------------------------------------------------------------------------
// world.hpp declares the shape (`world::procurement_quotes`,
// `world::procurement_contracts`, `world::next_procurement_id`);
// components.hpp declares the two records. This is
// the procurement seam's leg of the flat-binary serialisation seam — the
// FOURTH such stream in world/*, after history_log, order_book and (in
// spirit) the BL-107 magic+version header they both follow: leading magic +
// version, count-prefixed records, rejection rather than reinterpretation on
// anything that does not parse. See order_book.hpp for the fuller rationale;
// this header does not repeat it.
//
// Deliberately standalone, for history_log.hpp's/order_book.hpp's reason: the
// command seam (corp_command.cpp) and the tick's own contract-pacing pass
// (economy_system.cpp) touch quotes/contracts through world.hpp alone, so
// neither they nor the harnesses that link them need this translation unit.
// ---------------------------------------------------------------------------

/// Leading magic identifying a procurement stream: the bytes 'I','O','P','C'.
inline constexpr uint32_t procurement_magic =
    (uint32_t('I')) | (uint32_t('O') << 8) | (uint32_t('P') << 16) | (uint32_t('C') << 24);

/// Format version. Bump on any layout change to `procurement_quote` /
/// `procurement_contract` — the `static_assert`s on both record sizes
/// (components.hpp) are the tripwire.
///
/// Bumped to 2 by BL-392, which added `delivery_body` and `freight_cost` to
/// both records — a v1 stream carries neither and cannot be reinterpreted as v2.
///
/// BUMPED TO 3 BY BL-546, WHICH REMOVED THE TRAILING REPUTATION TABLE. That
/// table was never procurement's to hold: reputation is now the Trust dimension
/// of the relational substrate (`world::sentiment`), which carries its own leg
/// of the seam — `write_sentiment` / `read_sentiment` in sentiment.hpp, and the
/// whole-world path in world_save.cpp. Keeping a copy here would have recreated
/// the second store this migration exists to delete, and the two would drift the
/// first time anything but procurement moved the axis. A v2 stream ends with a
/// reputation block a v3 reader would consume as quote records; strict version
/// equality is what makes that impossible rather than merely unlikely.
inline constexpr uint32_t procurement_version = 3;

/// Sanity ceiling on each declared count, same reasoning as
/// `order_book_max_orders`: guards the eager reserve against a corrupt or
/// maliciously-crafted count before the per-record reads get a chance to fail
/// on a truncated stream.
inline constexpr uint32_t procurement_max_records = 1u << 20;

/// Write @p w's procurement state as: a 4-byte magic + a uint32_t version
/// header, then `next_procurement_id`, then quotes (count + records: id,
/// buyer, supplier, body, delivery_body, resource, quantity, unit_price,
/// lead_time_ticks, freight_cost), then contracts (count + records: the same
/// fields plus ticks_elapsed and deposit_paid). Binary, native byte order —
/// the project's settled flat-binary convention. STORED ORDER IS PRESERVED,
/// matching order_book's own rule (insertion order is meaningful — the oldest
/// quote a buyer holds against a supplier is still the first one accept_quote
/// would find by a linear id search).
///
/// NO RELATIONAL VALUE CROSSES THIS STREAM (BL-546, v3). Reputation used to
/// trail the contracts as a third block; it is now the Trust dimension of
/// `world::sentiment` and rides that table's own leg of the seam.
///
/// @param w   World whose procurement state is serialised.
/// @param out Binary output stream, positioned where the stream should start.
void write_procurement(const world& w, std::ostream& out);

/// Read a stream written by write_procurement back into @p w, REPLACING its
/// `procurement_quotes`, `procurement_contracts` and `next_procurement_id` on
/// success — and leaving `world::sentiment` untouched, which is not an omission
/// but the v3 layout (see above). Rejects (returns false, leaving all three
/// untouched) on a wrong magic, a wrong version, a truncated stream, a
/// resource byte that does not name a known `resource_type`, or a declared
/// count exceeding `procurement_max_records`.
///
/// @param w  World whose procurement state is replaced on success.
/// @param in Binary input stream, positioned where the stream should start.
/// @return   True on a fully-read, well-formed stream; false otherwise.
bool read_procurement(world& w, std::istream& in);
