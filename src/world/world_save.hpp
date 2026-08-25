#pragma once

#include "world.hpp"

#include <cstdint>
#include <iosfwd>

// ---------------------------------------------------------------------------
// World snapshot -- the flat-binary save (BL-536)
// ---------------------------------------------------------------------------
// The whole-world serialiser BL-107 has been blocked on since 2026-07-05, where
// it is named as "a WORLD-SNAPSHOT flat-binary serialiser... no backlog item
// exists for it yet". This is that serialiser, and the magic + version header
// below is BL-107's design realised on the stream it was always about.
//
// It is the FOURTH stream in the project and the first that covers the world
// rather than one slice of it. The other three -- `history_log` (IOHL),
// `order_book` (IOOB) and `procurement` (IOPC) -- established the convention
// this follows: magic first, version second, and a mismatch REJECTED rather
// than reinterpreted as a changed struct layout.
//
// WHAT IS IN THE STREAM, in three buckets, because the second is the one that
// bites (docs/development/backlog.json BL-536):
//
//   1. SERIALISED -- every authoritative container on `world`, plus the
//      well-known entities, the belt, and every id counter INCLUDING the
//      allocator cursor (world::next_entity_id).
//   2. REBUILT, never read -- `body_tile_index`, `astar_cost_cache`,
//      `logistics_flood_fields`, `body_reach_cost`, `body_market_index` and its two stamps, plus
//      `ai_decisions` and `current_day_tick`. Pure functions of what bucket 1
//      holds; writing them would only create a second thing to keep in
//      agreement. `clear_derived_state` is what a load calls.
//   3. NOT HERE -- the app-layer envelope (sim tick, world_params, the
//      generation report, the app histories, the ui_state slice). That lives
//      in `src/core/save_game.{hpp,cpp}`, deliberately: `world/*` stays SDL-
//      and UI-free, so the headless harness can exercise this half alone.
//
// corp_modifiers looks like bucket 2 and is NOT -- see the note on
// `write_world_snapshot`, and NR-510.
//
// The history log and the province partition are written by EMBEDDING the
// existing `write_history_log` / `read_history_log` pair whole, nested magic
// and all, rather than by re-writing their records here. Reuse over a second
// definition of intricate bytes (NR-512).
// ---------------------------------------------------------------------------

/// Leading magic identifying a world snapshot: the bytes 'I','O','S','V'.
/// `read_world_snapshot` rejects any stream not starting with exactly this,
/// rather than guessing at a layout (BL-107).
inline constexpr uint32_t world_save_magic =
    (uint32_t('I')) | (uint32_t('O') << 8) | (uint32_t('S') << 16) | (uint32_t('V') << 24);

/// Format version. BUMP THIS on any change to what is written or in what order
/// -- an added field, a reordered one, a widened enum, a change to
/// `resource_count`. `read_world_snapshot` rejects a mismatch rather than
/// attempting to reinterpret a differently-shaped record.
///
/// Broader versioning (migration, forward compatibility) stays deferred until
/// the data model stabilises, per docs/tech/TECH_FOUNDATIONS.md. For the
/// prototype, rejection IS the migration story.
///
/// Bumped to 2 by BL-546: `corp_reputation` (one float per pair) was replaced
/// in place by `sentiment` (two floats per pair, Access and Trust). A v1
/// snapshot's records are half the width, so every field after them would be
/// misread — which is exactly the case strict version equality exists for.
///
/// Bumped to 3 by Sprint N3 T2: `world::nation_budgets` (the persistent
/// per-nation weight map the national budget pass spends by) is written as a
/// new section directly after `nations`. A v2 stream has no such section, so
/// its `tile_to_nation` count would be read as the map's count and everything
/// after it misaligned; a v2 stream is therefore refused WHOLE, destination
/// untouched (that is the existing rejection contract, not a new one).
///
/// Bumped to 4 by BL-569 (province holder): `world::province_holder` (one
/// entity_id per province, positionally aligned with `provinces.provinces`)
/// is written as a new TRAILING section, directly after the embedded history
/// log + province partition stream (itself the previous last section). A v3
/// stream simply ends where this one continues, so it is refused whole on the
/// same strict-equality contract as every prior bump — there is no partial
/// read of a v3 stream as a v4 with an empty holder vector.
///
/// Bumped to 5 by BL-570 (condition_subject::province_held): NOT a new
/// trailing section this time — `condition` (condition_set.hpp) is a
/// FIXED-SIZE record embedded inside every law and every corp's embargo
/// condition_set (w_condition/r_condition), and it gained a new field
/// (`c.province`), so every condition record written by v4 is one field short
/// of a v5 record. A v4 stream is refused whole, on the same strict-equality
/// contract as every prior bump — reinterpreting it would misread whatever
/// bytes follow the first law's condition as that condition's province id.
///
/// Bumped to 6 by BL-571 (nation garrisons), landing the same Batch Delivery
/// wave as BL-570 above: `nation_component::capital_tile` (one entity_id) is
/// a new field IN THE MIDDLE of the existing per-nation record
/// (`w_nation`/`r_nation`), not a trailing section — every nation record
/// written before this item is one `w_id` short, so a v5 stream reads
/// misaligned from the first nation onward rather than merely truncated. The
/// same strict-equality contract refuses it whole. (BL-570 and BL-571 each
/// independently bumped to 5 in their own worktrees; integrating both landed
/// two new fields, so the version needed two bumps, not one — v5 exists only
/// as the intermediate BL-570-alone shape and was never itself released.)
///
/// Bumped to 7 by BL-572 (contract offers): `world::mercenary_offers` (open
/// mercenary-contract offers, one `mercenary_offer` record each) and
/// `world::next_offer_id` (its allocator cursor) are written as a new TRAILING
/// section, directly after `province_holder` (itself the previous last
/// section) — the same shape BL-569 used to add `province_holder` onto v3. A
/// v6 stream simply ends where this one continues, so it is refused whole on
/// the same strict-equality contract as every prior bump.
///
/// Bumped to 8 by BL-573 (contract record and verbs): `world::mercenary_contracts`
/// (accepted mercenary contracts, one `mercenary_contract` record each, fixed
/// at `mercenary_contract_max_units` committed-unit slots) and
/// `world::next_contract_id` (its allocator cursor) are written as a new
/// TRAILING section, directly after `mercenary_offers` (itself the previous
/// last section) — the same shape v7 used to add `mercenary_offers` onto v6.
/// A v7 stream simply ends where this one continues, so it is refused whole
/// on the same strict-equality contract as every prior bump.
///
/// Bumped to 9 by BL-585 (ancient goods append): four new `resource_type`
/// values (`ceramics`, `dressed_stone`, `planks`, `tools`) widen
/// `resource_count` 38 -> 42, which widens the length of EVERY per-resource
/// `std::array<T, resource_count>` in the stream (stockpiles, prices, tile
/// deposits, ...) rather than adding a trailing section — a structural
/// change, not an additive one. `max_resource` (this file's enum-ceiling
/// block) derives from `resource_count` automatically, so `r_enum` widens
/// with it; nothing else needs touching. A v8 stream's arrays are the wrong
/// length for a v9 reader, so it is refused whole on the same strict-equality
/// contract as every prior bump — there is no migration to write.
///
/// Bumped to 10 by BL-586 slice 2 (hides/fibre/leather/cloth/rigging): five
/// more new `resource_type` values widen `resource_count` 42 -> 47, the same
/// structural class of move as the v8 -> v9 bump above — every per-resource
/// array in the stream widens, not a trailing section. A v9 stream's arrays
/// are the wrong length for a v10 reader, so it is refused whole on the same
/// strict-equality contract as every prior bump.
///
/// Bumped to 11 by BL-612 (urban ground stamped): `world::land_use` (sparse
/// per-tile `land_use_component`, seeded at generation with the urban
/// footprints under population centres) is written as a new section directly
/// after `population_centre_name`. A v10 stream has no such section, so the
/// bytes where it would sit would be read as the nation store's count and
/// everything after it misaligned; a v10 stream is therefore refused whole,
/// destination untouched — the existing rejection contract.
///
/// Bumped to 12 by BL-611 (province centre anchor), landing the same wave as
/// BL-612 above (v11 exists only as the BL-612-alone shape, the BL-570/BL-571
/// precedent): `population_centre_component::province_anchor` is a new field
/// IN THE MIDDLE of the per-centre record (`w_popcentre`/`r_popcentre`), so
/// every centre record written before it is one field short and a v11 stream
/// reads misaligned from the first centre onward. Refused whole, same
/// strict-equality contract as every prior bump.
///
/// Bumped to 13 by BL-613 (qualification fraction): the nation record
/// (`w_nation` / `r_nation`) gains `nation_component::qualification` — one
/// float after `capital_tile`, in declaration order. A v12 stream simply
/// predates the field and has nowhere to source it from, so it is refused
/// whole on the same strict-equality contract as every prior bump. The reader
/// additionally refuses a non-finite or out-of-[0, 1] value: the writer
/// cannot have produced one, so the stream is corrupt rather than odd.
///
/// Bumped to 14 by BL-614 (wage competition): the building record
/// (`w_building` / `r_building`) gains `building_component::wage_bid` — one
/// float in declaration order, between `ai_cooldown` and
/// `recipe_switch_cooldown`. A v13 stream's building records are one f32 short, a MID-RECORD
/// gap, so it is refused whole on the same strict-equality contract as every
/// prior bump. The reader additionally refuses a non-finite or negative
/// value — the writer cannot have produced one.
/// (BL-613/BL-614 were authored as v11/v12 on their own branch and renumbered
/// to v13/v14 at the wave-1 integration, where BL-612/BL-611 held 11/12.)
///
/// Bumped to 15 by BL-624 (razed settlement tier): the population-centre
/// record (`w_popcentre` / `r_popcentre`) gains
/// `population_centre_component::razed` — one int after `province_anchor`, in
/// declaration order. A v14 stream's centre records are one field short, a
/// MID-RECORD gap, so it is refused whole on the same strict-equality
/// contract as every prior bump. The reader additionally refuses a value
/// other than 0/1 — the writer cannot have produced one.
inline constexpr uint32_t world_save_version = 15;

/// Write @p w as a complete world snapshot.
///
/// Writes magic + version, then every serialised container in a fixed order.
/// Derived caches are skipped (see `clear_derived_state`). `corp_modifiers` IS
/// written despite being described on `world` as derived: its stored ORDER is
/// the cross-tick earn order, which a re-fold from `earned_techs` x the gate
/// table cannot reconstruct, and `modified_scalar` folds in stored order over
/// operations that do not commute (NR-510).
///
/// Never fails by return value -- an unwritable stream surfaces through the
/// stream's own state, which the caller in `save_game.cpp` checks once for the
/// whole file rather than per field.
///
/// @param w   World to serialise.
/// @param out Destination stream, opened binary.
void write_world_snapshot(const world& w, std::ostream& out);

/// Read a world snapshot written by `write_world_snapshot` into @p w.
///
/// ATOMIC ON FAILURE. The stream is read into a scratch `world` and moved into
/// @p w only once every field has been read successfully, so a wrong magic, a
/// mismatched version, a corrupt length prefix or a truncated tail leaves @p w
/// EXACTLY as it was. A caller never has to distinguish "failed cleanly" from
/// "failed halfway".
///
/// On success the derived caches are already cleared, so the first read of any
/// of them rebuilds against the loaded world.
///
/// @param w  Destination world, untouched unless the whole read succeeds.
/// @param in Source stream, opened binary.
/// @return   True if the snapshot was read whole; false on any rejection.
bool read_world_snapshot(world& w, std::istream& in);

/// Drop every derived cache on @p w so the next read of each rebuilds.
///
/// Called by `read_world_snapshot` on success. Exposed because the round-trip
/// harness asserts the rebuild produces identical contents, which means it
/// needs to clear a freshly-generated world the same way a load does.
///
/// Clears: `body_tile_index`, `astar_cost_cache`, `logistics_flood_fields`, `body_reach_cost`,
/// `body_market_index` (and its count/max-id stamps), `ai_decisions`, and
/// `current_day_tick`. Does NOT touch `corp_modifiers` -- see above.
///
/// @param w World whose caches are dropped.
void clear_derived_state(world& w);
