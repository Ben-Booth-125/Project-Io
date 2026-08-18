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
//     region's grassland-ish farm endowment. Named in the row itself.
//   - GUNPOWDER has no saltpetre/sulphur signal. Proxy: the energy endowment.
// Both are recorded on the row so a reader can see what is standing in for what.

#include "combat.hpp"
#include "components.hpp" // resource_count, unit_component (the upkeep vector)
#include "entity.hpp"

#include <array>
#include <cstdint>
#include <vector>

struct region;
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
/// threshold on a region endowment window (0-1000); zero means "no gate".
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
std::vector<const roster_row*> available_rows(const region& p, roster_band band);

/// The campaign-side stand-in for a region's authored ground endowment
/// (BL-324, 2026-08-08 — the 1960 campaign has no regions to gate on). Read
/// from the corp's OWN stockpile and market access. Presence is binary by
/// design (1000, the table's own max threshold, or 0) — "you may field
/// rifles because you can buy steel" is a yes/no supply-chain question here,
/// not a graduated tuning knob; quantity-scaled readiness is a follow-on.
struct campaign_roster_gate_input
{
    int ore_q = 0, farm_q = 0, port_q = 0, energy_q = 0;
};

/// Sum @p corp's holding of @p res across its (corp, body) pools — the live
/// L3 store (world.hpp § corp_body_pools). Exported so both the gate above and
/// the hire verb's cost debit (corp_command.cpp) share one aggregation.
float corp_stockpile_total(const world& w, entity_id corp, resource_type res);

/// Derive @p corp's campaign gate input: pools summed across its bodies,
/// plus whether it holds a port anywhere.
campaign_roster_gate_input campaign_gate_input(const world& w, entity_id corp);

/// available_rows' campaign-side sibling — gates on @p corp's stockpile/market
/// access (via campaign_gate_input) instead of a region endowment. The
/// Era -1 sim is untouched: it keeps calling the region-gated overload above.
std::vector<const roster_row*> available_rows(const world& w, entity_id corp, roster_band band);

/// Compose an army stack from @p manpower over the rows @p p and @p band make
/// available, scaled by @p readiness_q (1000 = full). Returns an empty stack
/// for non-positive manpower.
std::vector<army_stack_entry> roster_stack(int64_t     manpower,
                                           const region& p,
                                           roster_band     band,
                                           int             readiness_q);

/// Map a polity's military capacity band (1-6, the ladder's own numbering) onto
/// its roster band. This is the ONE place the two numberings meet.
roster_band roster_band_for_capacity(int military_capacity);

// ===========================================================================
// BL-454 / BL-459 — standing-force upkeep and derived strength
// ===========================================================================
//
// Until BL-454, `w.units` appeared in economy_system.cpp, budget_system.cpp and
// construction.cpp exactly ZERO times: hire_unit debited once and the regiment
// was then free forever, while every building beside it paid maintenance and
// wages every tick. Upkeep is credits AND military goods (Ben, 2026-08-17), so
// the cost is a VECTOR from the start — a float with goods bolted on later is a
// rewrite, not an extension.
//
// The per-type data belongs to the ROSTER, so it lives here rather than being
// invented in the economy step. What the economy step owns is the PASS (drawing
// the pool, applying the decay); what the budget owns is the CREDIT term.

/// Per-tick standing-force upkeep rates, authored in scripts/economy.lua under
/// `economy.military.unit_upkeep`. Reached through
/// `recipe_registry::military().upkeep`.
///
/// EVERY RATE DEFAULTS TO ZERO. The item lands INERT: with the rates at zero no
/// credit is charged, no pool is drawn, no draw can go unmet, and (with
/// `out_of_supply_reach` at zero, which disables the reach trigger entirely)
/// no unit's supply factor ever moves. That is what lets the shipped economy
/// goldens be re-blessed honestly — the only thing that moves at zero rates is
/// the world state hash, because `unit_component` gained two fields.
struct unit_upkeep_params
{
    /// Credits per head per tick, flat. The wage half of the vector.
    float credits_per_head = 0.0f;

    /// Credits per head per tick, scaled by the roster row's `power_mod`
    /// (per-mille, 0..420 across the shipped table) — the same "floor plus a
    /// power-scaled component" shape BL-394 gave the hire cost, so a heavy row
    /// costs more to keep as well as more to raise.
    float credits_per_head_per_power = 0.0f;

    /// Goods drawn per head per tick, resource-indexed — the goods half of the
    /// vector, authored as an ordinary `{resource_name = qty}` table.
    ///
    /// ORDNANCE IS THE GOOD (Ben, 2026-08-17) — BL-457 added `resource_type::
    /// ordnance` as the roster's first terminal MILITARY good for exactly this
    /// consumer, and chain_depth.cpp's actor-consumed exemption names this pass
    /// by name. It is authored as DATA rather than hard-coded here so that
    /// naming a good is a one-line economy.lua change and never a code change;
    /// `food_rations` is the sanctioned second line. Both are authored at ZERO
    /// on landing — see the struct comment.
    std::array<float, resource_count> goods_per_head = {};

    /// The ONE decay rule's subtraction, per-mille of supply factor per tick.
    /// Applied when EITHER trigger fires: the unit is out of reach, or its goods
    /// draw went unmet. Same subtraction, same reason — not two rules.
    int supply_decay_permille = 0;

    /// Recovery per tick, per-mille, while neither trigger fires. Ceilinged at
    /// 1000 (fully supplied).
    int supply_recovery_permille = 0;

    /// Trigger (a) — BL-325 S3's out-of-supply decay. A unit whose tile's
    /// logistics reach cost exceeds this (or which is unreachable outright) is
    /// out of supply. ZERO OR BELOW DISABLES THE TRIGGER ENTIRELY, which also
    /// means the reach field is never built from the upkeep pass at the default
    /// rates — the Dijkstra cost is opt-in.
    float out_of_supply_reach = 0.0f;
};

/// One unit's resolved upkeep for one tick — the cost VECTOR, not a float.
struct unit_upkeep_draw
{
    float credits = 0.0f;                              ///< Charged by apply_budget (its own term).
    std::array<float, resource_count> goods = {};      ///< Drawn from the (owner, body) pool.
    bool  any_goods = false;                           ///< True iff some entry of `goods` is > 0.
};

/// Resolve @p u's upkeep for one tick against @p p and the roster row @p u.type
/// names. A `type` outside the table resolves against a zero-power row rather
/// than failing — an unknown roster index is a data problem, not a reason to
/// make a unit free.
unit_upkeep_draw resolve_unit_upkeep(const unit_component& u, const unit_upkeep_params& p);

/// The per-type quality scalar of BL-459's formula, per-mille (1000 = neutral).
/// DERIVED from the row's `power_mod` rather than authored as a second number,
/// so there is exactly one per-type quality figure and it cannot drift from the
/// one combat already reads. Levy Spear (power_mod 0) is 1000; Rifle Regiment
/// (power_mod 380) is 1380.
int roster_type_quality_permille(uint16_t roster_type);

/// BL-459. A unit's combat strength, DERIVED — there is no stored field:
///
///     strength = count x roster_type_quality x supply_factor   (x100 fixed point)
///
/// Integral throughout (the x100 fixed point makes that natural), so a caller
/// may accumulate strengths over an unordered container without the float
/// non-associativity that would make the sum order-dependent — which
/// condition_set.cpp's `military_strength` subject relies on.
///
/// SCALE ANCHOR: at quality 1.0 and full supply, `strength == count * 100`
/// exactly.
///
/// @param w Reserved. The roster is one global table today, so nothing here
///          reads the world; the parameter is the seam for the per-era /
///          per-polity roster BL-274 already anticipates, and it keeps every
///          call site from having to change when that lands.
int64_t unit_strength(const world& w, const unit_component& u);

/// BL-459's ADAPTER — the only place the roster meets combat. Both resolvers
/// (`resolve_battle`, `resolve_campaign_battle`) take fully-resolved
/// `army_stack_entry` values and deliberately never look a roster up
/// (combat.hpp's own stated boundary), and until now NOTHING converted a live
/// `unit_component` into one, so no unit in `w.units` could reach a battle at
/// all.
///
/// Supply is carried in the entry's `count` (the headcount that can actually be
/// put in the field) and quality in `type_power_mod` (the row's own modifier,
/// which is what the resolver's matchup expects). Deliberately NOT both in one
/// place: doubling quality into count as well would square it against
/// `unit_strength`.
army_stack_entry unit_to_stack_entry(const world& w, const unit_component& u);
