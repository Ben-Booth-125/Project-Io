#pragma once

// ---------------------------------------------------------------------------
// Era-keyed unit rosters (BL-274) — an authored TABLE, not a tree.
// Authority: docs/research/ANCIENT_TECH_LADDER.md § Shape (the roster grouping).
// ---------------------------------------------------------------------------
//
// A polity's available unit types are a pure function of its ground and its
// era band. No research, no player choice, no unlock events: availability is
// DERIVED, so a roster is explainable from the map exactly the way ideology is
// (BL-274's own framing).
//
// FILE PLACEMENT DEVIATES FROM THE ITEM. BL-274 names combat.{hpp,cpp} as its
// home, but combat.hpp states the opposite boundary in its own words — an
// army_stack_entry is "deliberately NOT a lookup key into a roster table". The
// engine scores whatever stack it is handed and knows nothing about where the
// units came from, which is the property that lets the 1960 era share it. So
// the roster lives here and RESOLVES INTO combat's types, rather than combat
// growing a table it was designed not to have.
//
// THE FOUR BANDS are the roster grouping settled 2026-08-04 (and the answer to
// this item's open question 1, which leaned four): classical = ladder T1,
// medieval = T2-T3, gunpowder = T4, industrial = T5-T6. Keyed off the ladder's
// Military column, because that is the column whose rows must turn over exactly
// at a roster boundary.
//
// NAMED SUBSTITUTIONS (BL-221's convention: name the stand-in, never fake the
// input). Two material signals do not exist yet and are read through proxies:
//   - HORSES have no pasture or domesticable-clade axis at all. Proxy: the
//     province's grassland-ish farm endowment. Named in the row itself.
//   - GUNPOWDER has no saltpetre/sulphur signal. Proxy: the energy endowment.
// Both are recorded on the row so a reader can see what is standing in for what.

#include "combat.hpp"
#include "entity.hpp"

#include <cstdint>
#include <vector>

struct province;
struct world;

/// The four roster bands. Values are the ladder's own grouping, so a band index
/// converts to ladder bands via ANCIENT_TECH_LADDER.md's roster_bands table.
enum class roster_band : uint8_t
{
    classical  = 0, ///< Ladder T1. Massed iron infantry, siegecraft.
    medieval   = 1, ///< Ladder T2-T3. Stirrup heavy cavalry, fortress, crossbow/plate.
    gunpowder  = 2, ///< Ladder T4. Flintlock line, artillery fortress, broadside fleet.
    industrial = 3, ///< Ladder T5-T6. Rifle, ironclad -> armour, airpower.
};

inline constexpr int roster_band_count = 4;

/// The 1960s campaign's fixed roster band (BL-324, 2026-08-08) — not derived
/// from a military-capacity score (that's the Era -1 settlement model); the
/// campaign is simply industrial-era throughout, and bands are cumulative, so
/// this still exposes every earlier-band row a corp's ground can support.
inline constexpr roster_band campaign_roster_band = roster_band::industrial;

/// What a row needs from the ground before a polity can field it. Each is a
/// threshold on a province endowment window (0-1000); zero means "no gate".
struct roster_gate
{
    int ore_q   = 0; ///< Metallurgy: iron, then steel.
    int farm_q  = 0; ///< PROXY FOR PASTURE — horses have no signal of their own.
    int port_q  = 0; ///< Coastal: marines and fleets.
    int energy_q = 0; ///< PROXY FOR SALTPETRE, then genuinely fuel from T5.
};

/// One fieldable unit type.
struct roster_row
{
    const char* name;      ///< Generic mechanism name. Never an Earth proper noun.
    roster_band band;
    unit_class  cls;
    roster_gate gate;

    /// Per-mille modifier this type carries into `army_stack_entry::type_power_mod`,
    /// layered on its class's base power (combat.hpp's stated extension point).
    int power_mod;

    /// Relative weight when composing a stack from the available rows. Not a
    /// count — the share each available row takes of the raised manpower.
    int weight;
};

/// The whole table, in band order. Exposed so a harness can assert over it
/// rather than re-deriving what it thinks the table says.
const std::vector<roster_row>& unit_roster_table();

/// The rows @p p's ground and @p band make available. ASYMMETRY IS THE POINT
/// (BL-274): two polities at the same date field different rosters because
/// their ground differs, so this is the function that makes belief-and-
/// environment-onto-war visible.
///
/// Bands are cumulative: a gunpowder-band polity still fields infantry. A row
/// from an EARLIER band stays available, because nothing un-invents a spear.
std::vector<const roster_row*> available_rows(const province& p, roster_band band);

/// The campaign-side stand-in for a province's authored ground endowment
/// (BL-324, 2026-08-08 — the 1960 campaign has no provinces to gate on). Read
/// from the corp's OWN stockpile and market access. Presence is binary by
/// design (1000, the table's own max threshold, or 0) — "you may field
/// rifles because you can buy steel" is a yes/no supply-chain question here,
/// not a graduated tuning knob; quantity-scaled readiness is a follow-on.
struct campaign_roster_gate_input
{
    int ore_q = 0, farm_q = 0, port_q = 0, energy_q = 0;
};

/// Sum @p corp's stockpile of @p res across every building it owns. Buildings
/// carry the stockpile, not the corp; exported so both the gate above and the
/// hire verb's cost debit (corp_command.cpp) share one aggregation.
float corp_stockpile_total(const world& w, entity_id corp, resource_type res);

/// Derive @p corp's campaign gate input: stockpiles summed across its owned
/// buildings, plus whether it holds a port anywhere.
campaign_roster_gate_input campaign_gate_input(const world& w, entity_id corp);

/// available_rows' campaign-side sibling — gates on @p corp's stockpile/market
/// access (via campaign_gate_input) instead of a province endowment. The
/// Era -1 sim is untouched: it keeps calling the province-gated overload above.
std::vector<const roster_row*> available_rows(const world& w, entity_id corp, roster_band band);

/// Compose an army stack from @p manpower over the rows @p p and @p band make
/// available, scaled by @p readiness_q (1000 = full). Returns an empty stack
/// for non-positive manpower.
std::vector<army_stack_entry> roster_stack(int64_t     manpower,
                                           const province& p,
                                           roster_band     band,
                                           int             readiness_q);

/// Map a polity's military capacity band (1-6, the ladder's own numbering) onto
/// its roster band. This is the ONE place the two numberings meet.
roster_band roster_band_for_capacity(int military_capacity);
