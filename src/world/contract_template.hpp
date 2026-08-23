#pragma once

#include "condition_set.hpp"

#include <cstdint>
#include <string>
#include <vector>

class lua_state; // forward-declared: only load_from_lua needs it, and only the
                  // .cpp needs the Lua/sol2 headers -- this header stays inside
                  // the SDL/Lua-free world superset, exactly like
                  // recipe_registry.hpp does for the same reason.

// ---------------------------------------------------------------------------
// contract_template (BL-570) — one authored mercenary-contract KIND
// ---------------------------------------------------------------------------
// docs/economy/CONTRACTS.md § The mercenary contract: "The variety of contract
// types is authored data in the predicate, not a type enum in C++ ... adding a
// contract kind is a Lua change" to scripts/contracts.lua. A `mercenary_contract`
// (BL-573) will store an INDEX into this table, never a free `condition_set`
// (CONTRACTS.md § Serialisation) -- so this struct is what a "kind" IS, and the
// registry below is its authored roster.
//
// `predicate` is a SINGLE condition, not a `condition_set` AND-list: every
// template shipped so far names one fact (hold this province), and
// CONTRACTS.md's "condition_set" framing is the conceptual shape a contract
// generalises toward, not a literal type this narrow slice commits to widening
// yet. `predicate.province` is deliberately left at `no_province` here -- the
// PROVINCE is bound per accepted offer (BL-572 derives the offer, BL-573 binds
// it into a live contract), never authored in the template, so the same "take"
// row works for every province a client ever offers.
struct contract_template
{
    /// Stable slug ("take", "hold") a saved contract's template reference
    /// resolves back to via `contract_template_registry::index_of` -- so the
    /// STORED index stays meaningful even if `scripts/contracts.lua` reorders
    /// its rows, as long as the id survives (an id that stops existing is a
    /// save that can no longer resolve its template, same as any other authored
    /// reference going stale).
    std::string id;

    /// Human-facing label ("Take the Province").
    std::string name;

    /// The single fact the client pays to have become true. See the struct
    /// comment above for why this is one `condition`, not a `condition_set`.
    condition predicate;

    /// false ("take"): the predicate need only hold AT the deadline -- losing
    /// and retaking the province before then still completes it. true
    /// ("hold"): the predicate must hold on EVERY tick up to the deadline, or
    /// the contract fails early. CONTRACTS.md § Q2 names this as the
    /// completed/failed distinction's finer grain for the mercenary side.
    bool continuous = false;

    /// Multiplier the offer-derivation pass (BL-572, `derive_contract_offers`)
    /// applies to the base fee this kind of work commands.
    float fee_mult = 1.0f;

    /// Ticks from acceptance to the deadline.
    int deadline_ticks = 0;
};

/// Loads and holds `scripts/contracts.lua`'s authored `contract_templates`
/// table -- the live-Lua counterpart to `recipe_registry` (recipe_registry.hpp),
/// same split: this header is Lua-free so the SDL/Lua-free world superset can
/// still include it, and `contract_template.cpp` (which needs `<sol/sol.hpp>`
/// and `scripting/lua_state.hpp`) is excluded from that superset's build the
/// same way `recipe_registry.cpp` is (see CMakeLists.txt's `IO_WORLD_SOURCES`
/// filter).
class contract_template_registry
{
public:
    /// Load `scripts/contracts.lua`'s global `contract_templates` table
    /// through @p lua's protected sol2 path. Throws `std::runtime_error`,
    /// naming the offending row and field, on anything malformed -- an unknown
    /// subject/comparator name, a missing predicate, a non-finite operand, or a
    /// missing/duplicate `id`. Replaces any previously loaded roster.
    void load_from_lua(lua_state& lua);

    std::size_t              size() const { return m_templates.size(); }
    const contract_template& at(std::size_t index) const { return m_templates.at(index); }

    /// Index of the template whose `id` matches @p id, or -1 if none does --
    /// the round trip a saved contract's template reference resolves through.
    int index_of(const std::string& id) const;

private:
    std::vector<contract_template> m_templates;
};
