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
    std::vector<opening_kit_slot_record> slots; ///< in slot order (flex slots after the fixed extraction)
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
