#pragma once

#include "era_timelapse.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// polity_identity — what a realm carries BY ID across the wizard's rounds
// ---------------------------------------------------------------------------
//
// STARTUP.md § Identity across the rounds (Ben, 2026-09-24): a realm is one
// thing from the round that founds it to the nation Begin makes of it, and the
// map says so by carrying its colour slot, its shade rung, its seat and its
// name by id rather than re-deriving any of them per round. This header is the
// PURE half of that: functions over `era_timelapse` and region geometry alone,
// with no ImGui, no `world`, no settlement type — so the wizard's rounds
// (history_lapse.cpp), the Begin/load nation table (app.cpp) and the sweep's
// clash column (history_sweep.cpp) all call ONE assignment and cannot drift.
// The same argument era_timelapse.hpp makes for itself.
//
// WHAT A SLOT IS (BL-1087). A colour slot encodes a hue-family WEDGE and an
// OFFSET inside it: `slot = family * polity_slot_offsets + offset`. The family
// is the founding culture's root cradle (the lineage palette's wedge, BL-919),
// so kin realms sit in one wedge and read as kin; the offset is what the
// greedy walk hands out against the realm's neighbours, so two adjacent kin
// never share a swatch. A record with no culture tree at all has no wedges
// (`family_count == 0`) and takes the plain fallback slots `0..N-1`.
//
// PINS. A round hands the next its slots, rungs and dead ground (`polity_pins`);
// the successor's assignment keeps every pinned slot, colours only the rest,
// and resolves a clash between two pinned realms that this record makes
// neighbours by re-slotting the one with the smaller people share at the
// round's opening step. A dead realm's slot is retired only for a newcomer
// seated inside its last-held ground.
//
// DETERMINISTIC AND PURE: every walk is over vectors in index order.
// ---------------------------------------------------------------------------

/// Offsets one hue-family wedge holds. Six: the greedy walk hands out the
/// lowest free one, so kin neighbours take offsets 0 and 1 — a hue nudge plus a
/// lightness step apart — and the wedge rarely runs past three.
inline constexpr int polity_slot_offsets = 6;

/// Slots the fallback palette (no culture tree) holds — the width of the
/// pre-BL-1087 table in presentation.cpp.
inline constexpr int polity_fallback_slot_count = 20;

/// The wedge a slot sits in, or -1 for an unassigned slot.
inline int polity_slot_family(int slot) { return slot < 0 ? -1 : slot / polity_slot_offsets; }
/// The offset inside its wedge, or -1 for an unassigned slot.
inline int polity_slot_offset(int slot) { return slot < 0 ? -1 : slot % polity_slot_offsets; }

/// THE LINEAGE TREE'S WEDGES (the lineage palette's rule, BL-919, as a pure
/// walk): per culture, the compact index of its root cradle's wedge — the k-th
/// unfolded root in index order owns wedge k — and the wedge count. A folded
/// culture (BL-1017) wears its absorber's wedge; a daughter its parent's; a
/// malformed link (a parent at or above its daughter) is a root. Shared by the
/// wizard's lineage palette and the sweep, so both hand a polity one family.
struct lineage_wedges
{
    std::vector<int32_t> wedge; ///< Per culture: its wedge, or -1 for an empty tree.
    int count = 0;              ///< Wedges: the unfolded root cradles.
};
lineage_wedges lineage_wedges_of(const std::vector<int32_t>& parent,
                                 const std::vector<int32_t>* folded_into);

/// Tile -> nearest region index over LAND, -1 for water and for land no region
/// reached. A multi-source breadth-first walk from every region anchor with the
/// columns wrapping and the rows not — the walk `finish_history_lapse` always
/// made, lifted here so the sweep and the load path see the same raster.
/// @param water  One byte per tile in raster order, non-zero = water.
std::vector<int32_t> nearest_region_raster(const std::vector<uint8_t>& water, int gw, int gh,
                                           const std::vector<int32_t>& region_col,
                                           const std::vector<int32_t>& region_row);

/// Region adjacency off a nearest-region raster: two regions touch if any two
/// of their tiles do, under the same four-neighbour, column-wrapping rule the
/// raster was walked with. Sorted neighbour lists, each edge once per side.
std::vector<std::vector<int32_t>> region_adjacency(const std::vector<int32_t>& tile_region,
                                                   int gw, int gh, std::size_t region_count);

/// Polity -> the region the record FIRST shows it holding, -1 for an id the
/// record never shows holding ground. `changes` is in year order, so the first
/// appearance is the founding claim (or the resume's restatement).
std::vector<int32_t> polity_first_region(const era_timelapse& rec);

/// Polity -> its SEAT as the record states it: the region of its `founded` or
/// `inherited` event, else its first region. The founding seat is where a name
/// is coined and where the dead-ground rule looks.
std::vector<int32_t> polity_seat_region(const era_timelapse& rec,
                                        const std::vector<int32_t>& first_region);

/// Plurality culture per region at @p year — the `id[0]` of the last
/// `culture_change` at or before it — or -1 where the record says nothing.
/// Linear in `culture_changes`; the same cost a frame already pays for the
/// ownership slice.
std::vector<int32_t> culture_plurality_at(const era_timelapse& rec, int year);

/// Polity -> the culture of its founding, read UI-side (BL-1087): the plurality
/// people of its seat region at its first recorded year. -1 where the record
/// carries no culture change for that region by then.
std::vector<int32_t> polity_founding_culture(const era_timelapse& rec,
                                             const std::vector<int32_t>& seat_region,
                                             const std::vector<int32_t>& first_region);

/// The identity one round hands the next, all by polity id.
struct polity_pins
{
    std::vector<int32_t> slot;              ///< Per polity: its slot, or -1 for none.
    std::vector<int32_t> rung;              ///< Per polity: shade rungs earned so far.
    /// Per REGION: the dead pinned realm that last held it, or -1. This is
    /// what "a newcomer seated inside its last-held ground" is read against.
    std::vector<int32_t> region_dead_owner;
};

struct polity_identity_input
{
    const era_timelapse* rec = nullptr;
    /// Region adjacency (`region_adjacency`), indexed by region.
    const std::vector<std::vector<int32_t>>* region_nbrs = nullptr;
    /// Per polity: the compact hue-family (wedge) index, -1 unknown. nullptr
    /// or `family_count == 0` means no wedges: the fallback slots.
    const std::vector<int32_t>* family = nullptr;
    int family_count = 0;
    /// The predecessor's pins, or nullptr for a round with nothing behind it.
    const polity_pins* pins = nullptr;
};

struct polity_identity
{
    std::vector<int32_t> slot;         ///< Per polity: the slot assigned.
    std::vector<int32_t> rung_carry;   ///< Per polity: rungs carried in from the predecessor.
    /// Two per polity (`2 * p`, `2 * p + 1`): the years its named moments fire
    /// — `civilisation_formed` for it, and crossing the sweep's ROSE rule (a
    /// peak at least double its start and three regions more) — ascending,
    /// `INT32_MAX` where a moment never fires. Each fires at most once.
    std::vector<int32_t> ratchet_year;
    std::vector<int32_t> first_region; ///< `polity_first_region`.
    std::vector<int32_t> seat_region;  ///< `polity_seat_region`.

    // --- The instrument (BL-1087 R5): what the rules had to do -------------
    int pinned_clashes = 0; ///< Pinned pairs this record made neighbours while sharing a slot.
    int reslotted      = 0; ///< Pinned realms the clash rule moved (one per clash, the smaller share).
    int inherited_dead_slot = 0; ///< Newcomers seated in a dead realm's ground that took its slot.
    int spills         = 0; ///< Realms that found no free slot and took the least-used one.
};

/// The assignment. Pinned slots are kept, the clash rule runs first, then the
/// rest are coloured greedily in founding (id) order against the polity
/// adjacency folded from the whole record.
polity_identity assign_polity_identity(const polity_identity_input& in);

/// Shade rungs @p polity carries at @p year: its carry plus the named moments
/// fired at or before the year. 0 for an id outside the table.
int polity_rung_at(const polity_identity& id, std::size_t polity, int year);

/// The pins this record hands its successor: every slot (dead realms included,
/// so their slots stay retired), the rung each realm reached by the record's
/// end, and the last-held ground of every realm that died inside it, laid over
/// the predecessor's dead ground where that ground was not re-recorded.
polity_pins pins_from(const polity_identity& id, const era_timelapse& rec,
                      const polity_pins* predecessor);

/// One re-seating or founding, for the capital fold (BL-1088).
struct polity_capital_move
{
    int32_t  year   = 0;
    uint16_t polity = 0;
    uint16_t region = 0;
};

/// Every `founded`, `inherited` and `capital_moved` event as a move, ascending
/// by year (the event order), so `polity_capital_at` is one backward scan.
std::vector<polity_capital_move> polity_capital_moves(const era_timelapse& rec);

/// Where @p polity's seat stands at @p year: the last move at or before the
/// year, else @p fallback (its first region). THE NAME NEVER FOLLOWS THIS.
int32_t polity_capital_at(const std::vector<polity_capital_move>& moves, uint16_t polity,
                          int year, int32_t fallback);
