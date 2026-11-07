#pragma once

// ---------------------------------------------------------------------------
// opening_kit.hpp — BL-1473, A MAJOR'S OPENING KIT
// ---------------------------------------------------------------------------
// CORPORATION_GENERATION.md § A major's opening kit (Ben, 2026-11-02): every
// major opens on the same kit; a minor opens on the walk. At the finish, after
// the colonial ports and before the site roads and the Logistic Point seeding
// (so both road and cover the kit), every major's homeworld ground is cleared of
// the extraction, processing and civic buildings industry generation left there
// (THE TAKE-UP), and the active kit (`world_gen.opening_kit`) is laid in their
// place, owned by the nation. Every kit placement passes § Pass 3's rules
// unchanged (own ground, can_place, chain-feasible, sized to spare input, bounded
// by want); a slot the ground cannot hold walks the substitution ladder and,
// failing it, stays unbuilt and is recorded here. Nothing is minted. Uses no RNG;
// every walk is over sorted ids.

#include "components.hpp"
#include "world_gen_config.hpp"

#include <string>
#include <vector>

struct world;
class recipe_registry;

/// One kit slot's outcome on one major's ground.
struct opening_kit_slot_record
{
    enum class outcome : std::uint8_t { placed, substituted, unbuilt };
    kit_slot_category category = kit_slot_category::extraction;
    std::string       slot;     ///< the slot as authored ("coal", "flex", "steel", "schooling")
    outcome           result = outcome::unbuilt;
    std::string       placed_as; ///< what stands: a good, a recipe or a building name ("" if unbuilt)
    std::string       note;      ///< the ladder rung taken, or why it is unbuilt
    entity_id         building = null_entity;
};

/// One major's kit.
struct opening_kit_major
{
    entity_id nation   = null_entity;
    int       taken_up = 0;     ///< buildings the take-up removed from its ground
    int       placed = 0, substituted = 0, unbuilt = 0;
    int       proc_built = 0; ///< kit processors standing once the kit is laid
    /// What refused the processing slots as named, summed over every slot and
    /// place read (`proc_refusals`): the diagnostic the kit's counts are tuned on.
    int       refused_no_site = 0, refused_not_wanted = 0, refused_no_feed = 0, refused_not_sized = 0;
    std::vector<opening_kit_slot_record> slots; ///< in slot order (flex slots after the fixed extraction)

    // A MARKETPLACE ON THE MILL'S MARKET (Ben, 2026-11-07, BL-1545), per steel slot.
    int mill_marketplace_placed  = 0; ///< a Marketplace laid on the slot's market
    int mill_marketplace_held    = 0; ///< the market already held one of the kit's
    int mill_marketplace_no_slot = 0; ///< the steel slot stood no building
    int mill_marketplace_no_site = 0; ///< no free own tile in that market admitted one

    // THE LOGISTICS CAP (`cap_major_logistics`), after the Logistic Point seeding.
    int nodes_source  = 0; ///< nodes on the capital tile, kept as the capital's source
    int nodes_kept    = 0; ///< hubs, stations and ports kept in slot
    int nodes_for_kit = 0; ///< of those, kept first for the kit buildings they cover
    int nodes_taken   = 0; ///< taken up beyond the count
    int nodes_laid    = 0; ///< laid into a slot the seeding left open
    int nodes_open    = 0; ///< slots left open (nothing on the ladder places)
    int resited       = 0; ///< kit buildings the cap left uncovered, laid again on covered ground
    int resite_failed = 0; ///< of those, ones no covered tile took (they stay where they stood)
    int uncovered     = 0; ///< the nation's buildings without Logistic Point cover after the cap
    std::string logistics_note;
};

/// What `lay_major_opening_kits` did.
struct opening_kit_report
{
    std::string kit;              ///< the active configuration's name
    int         majors   = 0;
    int         taken_up = 0;     ///< buildings removed from majors' ground, all majors
    int         charters_emptied = 0; ///< charter records left with no building (their opening stock lifted)
    int         placed = 0, substituted = 0, unbuilt = 0;
    std::vector<opening_kit_major> per_major; ///< ascending nation id

    /// The record for @p nation, or null.
    const opening_kit_major* of(entity_id nation) const
    {
        for (const opening_kit_major& m : per_major)
            if (m.nation == nation)
                return &m;
        return nullptr;
    }
};

/// Take up and lay every major's opening kit (see the header block). Throws
/// std::runtime_error when a processing slot names a recipe the registry does
/// not know.
opening_kit_report lay_major_opening_kits(world& w, const recipe_registry& reg,
                                          const opening_kit_config& kit);

/// Print the `[opening_kit]` summary line and one line per major.
void print_opening_kit(const opening_kit_report& r);

/// THE LOGISTICS CAP (CORPORATION_GENERATION.md § A major's opening kit, "the count is
/// a cap", Ben 2026-11-03). Run after `seed_logistics_sources`. On each major's ground
/// the nodes on the capital tile stay as its source. Then the standing node that brings
/// the most kit buildings under cover is kept, repeatedly, while one adds any (Ben,
/// 2026-11-04); the other ports, stations and hubs are walked in the order the seeding lays out from the capital (road chain
/// distance from the capital, unreached last, then tile id, then building id) and
/// each kept while its type's count has room, the rest taken up. A slot still open
/// is laid nearest the capital where placement allows, down its ladder (a port on
/// coastless ground a second station, a station without track a hub). Then counts
/// each major's buildings left without Logistic Point cover (the risk the cap
/// carries; nothing is re-added for it). No-op when @p kit has no logistics counts.
void cap_major_logistics(world& w, const recipe_registry& reg, const opening_kit_config& kit,
                         opening_kit_report& rep);

/// Print one `[opening_kit] logistics` line per major.
void print_opening_kit_logistics(const opening_kit_report& r);

/// THE FLAT TREASURY'S FOUNDINGS (cold review, BL-1473). The state handover's
/// foundings (a plant, a coastal port) record what each major PAID from its history's
/// chest, and the opening reserve refunds that payment. Once the kit stands and the
/// flat treasury replaces that chest, neither holds: a founding whose building the
/// take-up or the cap removed is gone (building nulled, nothing paid), and a major's
/// remaining foundings were paid from a chest that no longer exists (nothing paid). So
/// the reserve neither refunds into the flat figure nor annotates a building that is
/// gone. Call right after the flat treasury is set.
struct state_handover_report;
void release_kit_foundings(world& w, state_handover_report& handover);

/// A nation's buildings standing without Logistic Point cover: every Logistic
/// Point consumer it owns whose tile reads coverage 0 from its running sources.
/// @param include_idled Also count a standing extraction site or processor the
///        settle idled or the state left unfunded - the ones a lack of cover idles,
///        which `lp_consumer` no longer counts (BL-1473 R4).
int count_uncovered_buildings(world& w, entity_id nation, bool include_idled = false);
