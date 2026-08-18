#include "works_roster.hpp"

#include "scripting/lua_state.hpp"

#include <sol/sol.hpp>

#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>

// ---------------------------------------------------------------------------
// The ONE translation unit that pulls Lua for the works table (BL-321).
// Mirrors recipe_registry.cpp: works_roster.hpp stays pure data so the world
// superset and every headless harness link without this file.
//
// VALIDATION LIVES HERE, and that is the point of the file. Moving the table
// into Lua (Ben, 2026-08-07, NR-081) moved it out of the compiler's reach: a
// typo'd band, a row that does nothing, a duplicated name or a reach work no
// ground can gate would all now be runtime data rather than a build error. So
// the loader re-imposes what the compiler used to, and throws on the way in.
// A bad edit to works.lua fails loudly at startup instead of quietly producing
// a degraded world nobody notices for a month.
// ---------------------------------------------------------------------------

namespace
{

roster_band band_from_name(const std::string& name, bool& ok)
{
    static const std::unordered_map<std::string, roster_band> table = {
        { "classical",  roster_band::classical  },
        { "medieval",   roster_band::medieval   },
        { "gunpowder",  roster_band::gunpowder  },
        { "industrial", roster_band::industrial },
    };
    const auto it = table.find(name);
    ok = (it != table.end());
    return ok ? it->second : roster_band::classical;
}

/// Read an optional integer field, defaulting to 0. Absent is the normal case —
/// works.lua omits every axis a row does not gate on.
int opt_int(const sol::table& t, const char* key)
{
    const sol::optional<int> v = t[key];
    return v ? *v : 0;
}

int64_t opt_i64(const sol::table& t, const char* key)
{
    const sol::optional<int64_t> v = t[key];
    return v ? *v : 0;
}

bool any_effect(const work_effect& e)
{
    return e.capacity_mod || e.manpower_mod || e.reach_mod
        || e.defence_mod  || e.industrial_mod;
}

} // namespace

void works_registry::load_from_lua(lua_state& lua)
{
    sol::state& L = lua.state();

    const sol::optional<sol::table> works = L["works"];
    if (!works)
        throw std::runtime_error("works.lua: global 'works' is missing or not a table");

    m_rows.clear();

    std::set<std::string> seen_names;
    std::size_t index = 0;

    for (const auto& kv : *works)
    {
        ++index;
        const std::string where = "works.lua row " + std::to_string(index);

        const sol::optional<sol::table> row = kv.second.as<sol::optional<sol::table>>();
        if (!row)
            throw std::runtime_error(where + " is not a table");

        work_row r;

        // --- name ---------------------------------------------------------
        const sol::optional<std::string> name = (*row)["name"];
        if (!name || name->empty())
            throw std::runtime_error(where + " has no 'name'");
        r.name = *name;
        if (!seen_names.insert(r.name).second)
            throw std::runtime_error(where + ": duplicate work name '" + r.name + "'");

        // --- band ---------------------------------------------------------
        const sol::optional<std::string> band = (*row)["band"];
        if (!band)
            throw std::runtime_error(where + " ('" + r.name + "') has no 'band'");
        bool band_ok = false;
        r.band = band_from_name(*band, band_ok);
        if (!band_ok)
            throw std::runtime_error(where + " ('" + r.name + "'): unknown band '" + *band +
                                     "' — expected classical, medieval, gunpowder or industrial");

        // --- gate (every field optional; a row with no gate is legal) ------
        if (const sol::optional<sol::table> g = (*row)["gate"])
        {
            r.gate.ore_q      = opt_int(*g, "ore_q");
            r.gate.farm_q     = opt_int(*g, "farm_q");
            r.gate.port_q     = opt_int(*g, "port_q");
            r.gate.energy_q   = opt_int(*g, "energy_q");
            r.gate.population = opt_i64(*g, "population");
        }

        // --- effect -------------------------------------------------------
        if (const sol::optional<sol::table> e = (*row)["effect"])
        {
            r.effect.capacity_mod   = opt_int(*e, "capacity");
            r.effect.manpower_mod   = opt_int(*e, "manpower");
            r.effect.reach_mod      = opt_int(*e, "reach");
            r.effect.defence_mod    = opt_int(*e, "defence");
            r.effect.industrial_mod = opt_int(*e, "industrial");
        }
        if (!any_effect(r.effect))
            throw std::runtime_error(where + " ('" + r.name + "') has no effect at all — "
                                     "a work that does nothing is an authoring mistake, "
                                     "not a valid row");

        r.weight = opt_int(*row, "weight");
        if (r.weight <= 0)
            throw std::runtime_error(where + " ('" + r.name + "') has a non-positive 'weight'; "
                                     "a row with no pull can never be chosen");

        m_rows.push_back(std::move(r));
    }

    if (m_rows.empty())
        throw std::runtime_error("works.lua: the works table is empty");

    // A region records its works as a 32-bit mask (settlement.hpp § Era -1
    // works), so a 33rd row would be authored, validated, offered by
    // `available` — and then silently impossible to build, because
    // `apply_work_to_region` cannot address its bit. Caught here, where the
    // author is looking, rather than becoming a row that mysteriously never
    // appears in any world.
    if (m_rows.size() > works_mask_bits)
        throw std::runtime_error("works.lua: more than 32 works — region::works_built is a "
                                 "32-bit mask and could not address the extra rows");

    // --- table-level invariant -------------------------------------------
    // Every reach-bearing work must be raisable by SOME ground, and at least one
    // must be raisable by poor ground. reach_mod is how a polity buys its way
    // past BL-316's burden of breadth; if the only reach works sit behind rich
    // gates then poor ground cannot buy reach at all, and breadth silently
    // reverts to a ceiling rather than a decision. That is the exact failure
    // this item exists to prevent, so it is checked rather than assumed.
    bool any_reach = false, ungated_reach = false;
    for (const work_row& r : m_rows)
    {
        if (r.effect.reach_mod == 0) continue;
        any_reach = true;
        const work_gate& g = r.gate;
        if (g.ore_q == 0 && g.farm_q == 0 && g.port_q == 0 && g.energy_q == 0)
            ungated_reach = true;
    }
    if (!any_reach)
        throw std::runtime_error("works.lua: no work carries a 'reach' effect — a polity would "
                                 "have no way to buy its way past the burden of breadth");
    if (!ungated_reach)
        throw std::runtime_error("works.lua: every reach-bearing work is endowment-gated — "
                                 "a region with poor ground could never buy reach, which "
                                 "turns breadth back into a ceiling");
}
