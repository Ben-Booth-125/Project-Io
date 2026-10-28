#pragma once

// ---------------------------------------------------------------------------
// Unit designs and the kit catalogue (BL-1434; MILITARY.md § The kit)
// ---------------------------------------------------------------------------
// A unit is raised to a DESIGN: a land class with one kit item in each of the
// class's slots (Ben, 2026-10-27/28). The class fixes the role; the kit is what
// research has opened. A design's stats are its class's plus its kit's.
//
// THE CATALOGUE is an authored C++ table (unit_designs.cpp), beside the land-class
// stat table it adds to (unit_roster.cpp's k_land_stats) and authored the same
// way. It is VALIDATED AT COMPILE TIME (`kit_catalogue_first_error`, a
// static_assert in the .cpp), so a malformed row fails the build rather than a
// load; `kit_catalogue_first_error` is also callable at run time for a harness.
//
// THE STORE: a nation's live design per class is `nation_component::
// unit_design_kit` — the catalogue ids of its NON-BASE items only, so the default
// (base kit everywhere) is stored as nothing. A unit records a SNAPSHOT of the
// design it was raised to (`unit_component::kit`).
//
// THE LOCK RULE: a slot offers only researched kit. Base kit is always open. Every
// later item names the age-tree node that opens it (`kit_item::research_node`);
// the item is open for a nation that holds that node. The kit nodes are BL-1436's
// (kit in the age tree) to place, so today no named node is in the tree and every
// non-base item reads LOCKED.
// ---------------------------------------------------------------------------

#include "components.hpp"
#include "entity.hpp"
#include "land_combat_stats.hpp"
#include "unit_roster.hpp" // land_class

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

struct world;

/// "No kit item": an id outside the catalogue.
inline constexpr uint16_t k_no_kit_item = 0xFFFFu;

/// The goods one kit item names, per unit raised or refitted to it. Up to three.
struct kit_good
{
    resource_type res = resource_type::steel;
    float         qty = 0.0f;
};

inline constexpr std::size_t k_kit_goods_max = 3;

/// One catalogue row.
struct kit_item
{
    const char*       id;            ///< Stable string id, e.g. "inf.weapon.assault_rifle".
    const char*       name;          ///< Mechanism name for the designer. Never an Earth proper noun.
    land_class        cls;           ///< The class whose slot it fits.
    uint8_t           slot;          ///< The slot it fits, < kit_slot_count(cls).
    land_combat_stats delta;         ///< ADDED to the class's stats, before the row's quality scale.
    std::array<kit_good, k_kit_goods_max> goods; ///< Bought per unit raised or refitted to it.
    uint8_t           goods_count;   ///< Used entries of `goods`; 0 for base kit.
    const char*       research_node; ///< The age-tree node that opens it; nullptr for base kit.
};

// ---- The catalogue ---------------------------------------------------------

/// Every kit item, in catalogue-id order (the id is the index). Per (class, slot),
/// the base item comes first; the rest follow in the order the designer lists them.
const std::vector<kit_item>& kit_catalogue();

/// nullptr when the catalogue is well-formed, else the first rule it breaks. The
/// rules: every land class but `none` has its slots (MILITARY.md § The kit table);
/// each slot has exactly one base item, listed first, with no delta, no goods and
/// no research node; each slot has 2..4 later items, each with a research node and
/// at least one good of positive quantity; ids and research nodes are unique; a
/// hardness delta keeps the class inside 0..1000.
const char* kit_catalogue_first_error();

/// The number of kit slots @p c has: infantry 2, every other land class 3, `none` 0.
int kit_slot_count(land_class c);

/// The slot's name ("Weapon", "Tracks", ...); "" for a slot the class lacks.
const char* kit_slot_name(land_class c, int slot);

/// The catalogue ids that fit (@p c, @p slot), base first — the order of the
/// variants a design stores. Empty for a slot the class lacks.
const std::vector<uint16_t>& kit_items_for(land_class c, int slot);

/// The catalogue id of variant @p variant of (@p c, @p slot), or k_no_kit_item.
uint16_t kit_item_at(land_class c, int slot, uint8_t variant);

/// The catalogue id with string id @p id, or k_no_kit_item.
uint16_t kit_item_by_id(std::string_view id);

/// The variant (index within its slot's list) of catalogue item @p item; 0 for an
/// unknown item.
uint8_t kit_variant_of(uint16_t item);

// ---- The lock --------------------------------------------------------------

/// Whether @p nation has researched catalogue item @p item: base kit always; a
/// later item when the nation holds its research node (`nation_holds`). A node not
/// in the tree is held by nobody, so the item is locked (every non-base item today:
/// BL-1436 places the nodes). Unknown item: false.
bool kit_item_researched(const world& w, entity_id nation, uint16_t item);

/// HARNESS ONLY. Waive the lock above for every nation, so a headless check can
/// exercise an open slot before BL-1436 puts kit nodes in the tree. Process-global,
/// never saved, never set by the game; false by default.
void set_kit_research_waived_for_harness(bool waived);

// ---- Designs ---------------------------------------------------------------

/// @p nation's live design for @p c: base kit except where `unit_design_kit` names
/// an item. Unknown nation, or class `none`: all base.
unit_design design_of(const world& w, entity_id nation, land_class c);

/// The catalogue id fitted in @p slot of @p nation's live design for @p c, or
/// k_no_kit_item for a slot the class lacks.
uint16_t design_fitted(const world& w, entity_id nation, land_class c, int slot);

/// @p c's stats plus design @p d's kit: soft, hard, defence and breakthrough floored
/// at 0, hardness kept in 0..1000. Unscaled by any row's quality. A variant out of
/// range reads as base.
land_combat_stats kit_stats(land_class c, const unit_design& d);

/// `kit_stats` of @p nation's live design for @p c — the class's stats plus its kit's.
land_combat_stats design_stats(const world& w, entity_id nation, land_class c);

/// The goods of every non-base item in @p d, resource-indexed: what a hire to it buys.
std::array<float, resource_count> kit_goods(land_class c, const unit_design& d);

/// The goods a refit from @p from to @p to buys: each slot whose item changes buys
/// the new item's goods (a slot going back to base buys nothing).
std::array<float, resource_count> kit_refit_goods(land_class c, const unit_design& from,
                                                  const unit_design& to);

/// Is @p d a design @p c can hold — every variant inside its slot's list, and 0 past
/// the class's slots? The save loader's check.
bool design_valid(land_class c, const unit_design& d);

/// The land class @p u fights as (its row's; an unknown row fights as infantry).
land_class unit_land_class(const unit_component& u);

/// Does @p u's recorded design differ from its owner's live design for its class?
/// Such a unit may refit to the live design in place (`refit_unit` to its own row).
bool unit_design_stale(const world& w, const unit_component& u);
