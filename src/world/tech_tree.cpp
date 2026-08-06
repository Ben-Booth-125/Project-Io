#include "tech_tree.hpp"

#include "scripting/lua_state.hpp"

#include <sol/sol.hpp>

#include <stdexcept>
#include <string>

namespace {

/// Read an array-style Lua table of strings ({"a", "b"}) into a vector.
/// A missing table yields an empty list.
std::vector<std::string> read_string_list(const sol::optional<sol::table>& src)
{
    std::vector<std::string> out;
    if (!src)
        return out;
    for (std::size_t i = 1; i <= src->size(); ++i)
    {
        sol::optional<std::string> v = (*src)[i];
        if (v)
            out.push_back(*v);
    }
    return out;
}

} // namespace

void tech_tree_registry::load_from_lua(lua_state& lua)
{
    sol::state& s = lua.state();

    sol::optional<sol::table> root = s["tech_tree"];
    if (!root)
        throw std::runtime_error("tech_tree_registry: global 'tech_tree' table not found "
                                 "(was scripts/tech_tree.lua loaded?)");

    m_quests.clear();
    m_techs.clear();

    sol::optional<sol::table> quests = (*root)["quests"];
    if (quests)
    {
        for (std::size_t i = 1; i <= quests->size(); ++i)
        {
            sol::optional<sol::table> q = (*quests)[i];
            if (!q)
                throw std::runtime_error("tech_tree_registry: quest entry "
                                         + std::to_string(i) + " is not a table");
            tech_quest tq;
            tq.id       = q->get_or<std::string>("id", "");
            tq.name     = q->get_or<std::string>("name", "");
            tq.era      = q->get_or("era", 0);
            tq.type     = q->get_or<std::string>("type", "gate");
            tq.thread   = q->get_or<std::string>("thread", "");
            tq.thesis   = q->get_or<std::string>("thesis", "");
            tq.capstone = q->get_or<std::string>("capstone", "");
            tq.status   = q->get_or<std::string>("status", "stub");
            sol::optional<sol::table> opens = (*q)["opens"];
            tq.opens = read_string_list(opens);
            m_quests.push_back(std::move(tq));
        }
    }

    sol::optional<sol::table> techs = (*root)["techs"];
    if (techs)
    {
        for (std::size_t i = 1; i <= techs->size(); ++i)
        {
            sol::optional<sol::table> t = (*techs)[i];
            if (!t)
                throw std::runtime_error("tech_tree_registry: tech entry "
                                         + std::to_string(i) + " is not a table");
            tech_node tn;
            tn.id         = t->get_or<std::string>("id", "");
            tn.name       = t->get_or<std::string>("name", "");
            tn.short_name = t->get_or<std::string>("short_name", "");
            tn.quest      = t->get_or<std::string>("quest", "");
            tn.kind      = t->get_or<std::string>("kind", "invention");
            tn.tier      = t->get_or("tier", 0);
            tn.cost      = t->get_or<std::string>("cost", "M");
            tn.payoff    = t->get_or<std::string>("payoff", "");
            tn.condition = t->get_or<std::string>("condition", "research");
            tn.unlocks   = t->get_or<std::string>("unlocks", "");
            tn.status    = t->get_or<std::string>("status", "stub");
            sol::optional<sol::table> prereqs = (*t)["prereqs"];
            tn.prereqs = read_string_list(prereqs);
            m_techs.push_back(std::move(tn));
        }
    }
}
