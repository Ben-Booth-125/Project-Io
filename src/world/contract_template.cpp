#include "contract_template.hpp"

#include "scripting/lua_state.hpp"

#include <sol/sol.hpp>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

/// String <-> `condition_subject` names, for the authored `predicate.subject`
/// field. The full ten-entry vocabulary is mapped, not just `province_held` --
/// CONTRACTS.md's own rule ("adding a contract kind is a Lua change") only
/// holds if a future template row can name any subject `condition_set.hpp`
/// defines, not only the one this item happens to ship.
bool subject_from_name(const std::string& name, condition_subject& out)
{
    static const std::pair<const char*, condition_subject> table[] = {
        { "research",          condition_subject::research },
        { "structure",         condition_subject::structure },
        { "stockpile",         condition_subject::stockpile },
        { "market",            condition_subject::market },
        { "surplus",           condition_subject::surplus },
        { "era",               condition_subject::era },
        { "military_units",    condition_subject::military_units },
        { "military_strength", condition_subject::military_strength },
        { "science",           condition_subject::science },
        { "province_held",     condition_subject::province_held },
    };
    for (const auto& [key, value] : table)
        if (name == key) { out = value; return true; }
    return false;
}

/// String <-> `condition_comparator` names, for the authored
/// `predicate.comparator` field. All five are mapped for the same reason as
/// `subject_from_name` above.
bool comparator_from_name(const std::string& name, condition_comparator& out)
{
    static const std::pair<const char*, condition_comparator> table[] = {
        { "at_least",     condition_comparator::at_least },
        { "greater_than", condition_comparator::greater_than },
        { "exactly",      condition_comparator::exactly },
        { "at_most",      condition_comparator::at_most },
        { "less_than",    condition_comparator::less_than },
    };
    for (const auto& [key, value] : table)
        if (name == key) { out = value; return true; }
    return false;
}

/// Read one `predicate = { subject, comparator, operand }` table. Every field
/// is REQUIRED and REJECTED BY NAME on a bad value -- an authored table is as
/// untrusted as any other input this project range-gates (io-standing-rules.md
/// § Determinism & data model), and a silently-defaulted predicate would be a
/// contract kind that quietly does not test what its author intended.
condition read_predicate(const sol::table& tbl, const std::string& context)
{
    condition c;

    const sol::optional<std::string> subj = tbl["subject"];
    if (!subj)
        throw std::runtime_error(context + ".predicate.subject is missing");
    if (!subject_from_name(*subj, c.subject))
        throw std::runtime_error(context + ".predicate.subject '" + *subj
                                 + "' is not a known condition_subject");

    const sol::optional<std::string> cmp = tbl["comparator"];
    if (!cmp)
        throw std::runtime_error(context + ".predicate.comparator is missing");
    if (!comparator_from_name(*cmp, c.comparator))
        throw std::runtime_error(context + ".predicate.comparator '" + *cmp
                                 + "' is not a known condition_comparator");

    const sol::object operand_obj = tbl["operand"];
    if (!operand_obj.valid() || operand_obj.get_type() != sol::type::number)
        throw std::runtime_error(context + ".predicate.operand must be a number");
    const double operand = operand_obj.as<double>();
    if (!std::isfinite(operand))
        throw std::runtime_error(context + ".predicate.operand is not finite");
    c.operand = static_cast<float>(operand);

    // The province qualifier is bound per accepted offer (BL-572/BL-573), never
    // authored here -- see the struct comment in contract_template.hpp.
    c.province = no_province;

    return c;
}

} // namespace

void contract_template_registry::load_from_lua(lua_state& lua)
{
    sol::state& s = lua.state();

    const sol::optional<sol::table> templates_tbl = s["contract_templates"];
    if (!templates_tbl)
        throw std::runtime_error("contract_template_registry: global 'contract_templates' table "
                                 "not found (was scripts/contracts.lua loaded?)");

    std::vector<contract_template> loaded;
    loaded.reserve(templates_tbl->size());
    for (std::size_t i = 1; i <= templates_tbl->size(); ++i)
    {
        const sol::optional<sol::table> entry = (*templates_tbl)[i];
        if (!entry)
            throw std::runtime_error("contract_template_registry: template entry "
                                     + std::to_string(i) + " is not a table");

        contract_template t;
        t.id = entry->get_or<std::string>("id", "");
        if (t.id.empty())
            throw std::runtime_error("contract_template_registry: template entry "
                                     + std::to_string(i) + " has no 'id'");
        for (const contract_template& seen : loaded)
            if (seen.id == t.id)
                throw std::runtime_error("contract_template_registry: duplicate template id '"
                                         + t.id + "'");

        t.name = entry->get_or<std::string>("name", t.id);

        const sol::optional<sol::table> pred = (*entry)["predicate"];
        if (!pred)
            throw std::runtime_error("contract_template_registry: template '" + t.id
                                     + "' has no 'predicate' table");
        t.predicate = read_predicate(*pred, "contract_templates[" + std::to_string(i)
                                            + "] ('" + t.id + "')");

        t.continuous     = entry->get_or("continuous", false);
        t.fee_mult       = entry->get_or("fee_mult", 1.0f);
        t.deadline_ticks = entry->get_or("deadline_ticks", 0);

        loaded.push_back(std::move(t));
    }

    m_templates = std::move(loaded); // replace only after every row parses clean
}

int contract_template_registry::index_of(const std::string& id) const
{
    for (std::size_t i = 0; i < m_templates.size(); ++i)
        if (m_templates[i].id == id)
            return static_cast<int>(i);
    return -1;
}
