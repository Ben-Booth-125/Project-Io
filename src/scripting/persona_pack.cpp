#include "persona_pack.hpp"

#include "world/corp_ai.hpp"

#include <sol/sol.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

namespace persona {

namespace {

// Underscore tokens, matching what the pack Lua code matches against
// (`f.provenance == "own_asset"`) — distinct from corp_ai's hyphenated
// to_jsonl() names, which serve a different consumer (the JSONL file).
const char* provenance_token(fact_provenance p)
{
    switch (p)
    {
    case fact_provenance::own_asset:     return "own_asset";
    case fact_provenance::public_market: return "public_market";
    case fact_provenance::rival_visible: return "rival_visible";
    case fact_provenance::survey:        return "survey";
    case fact_provenance::route:         return "route";
    }
    return "unknown";
}

std::string require_string(const sol::table& t, const char* key, const char* what)
{
    sol::optional<std::string> v = t.get<sol::optional<std::string>>(key);
    if (!v)
        throw std::runtime_error(std::string("persona pack ") + what + ": '" + key
                                 + "' missing or not a string");
    return *v;
}

// ---------------------------------------------------------------------------
// Schema bridge — corp_ai.cpp's real predicate names (BL-202/206) vs the
// dotted names the Pantheon-sourced packs' extractors match on
// (persona-runtime.md's own convention). The packs are shipped data compiled
// from the Pantheon seeds independently of corp_ai's export; rather than
// editing either side, this loader translates. own/rival building facts fold
// onto the SAME dotted predicate (packs discriminate ownership via
// `provenance`, not the name), and body_activity's numeric activity_vis value
// becomes the tier string amaterasu.lua matches on.
// ---------------------------------------------------------------------------
std::string bridge_predicate(const std::string& raw)
{
    static const std::pair<const char*, const char*> kMap[] = {
        { "cash",                      "corp.cash" },
        { "building_type",             "building.type" },
        { "rival_building_type",       "building.type" },
        { "building_tile",             "building.tile" },
        { "rival_building_tile",       "building.tile" },
        { "rival_building_owner",      "building.owner" },
        { "building_recipe",           "building.recipe" },
        { "building_workforce_target", "building.workforce_target" },
        { "building_decommissioned",   "building.decommissioned" },
        { "body_activity",             "body.activity" },
    };
    for (const auto& [from, to] : kMap)
        if (raw == from)
            return to;
    return raw; // resource-keyed predicates (price:/pool:/tile_deposit:) pass through
}

// body_activity's raw value is the numeric activity_vis (world.hpp); amaterasu
// matches string tier names. Returns the tier name, or an empty string when
// `predicate` isn't body.activity (caller keeps the numeric value as-is then).
std::string bridge_activity_tier(double raw_value)
{
    switch (static_cast<int>(raw_value))
    {
    case 0: return "Unknown";
    case 1: return "Stale";
    case 2: return "Known";
    case 3: return "Visible";
    default: return "Unknown";
    }
}

std::string opt_string(const sol::table& t, const char* key)
{
    sol::optional<std::string> v = t.get<sol::optional<std::string>>(key);
    return v ? *v : std::string();
}

int require_int(const sol::table& t, const char* key, const char* what)
{
    sol::optional<int> v = t.get<sol::optional<int>>(key);
    if (!v)
        throw std::runtime_error(std::string("persona pack ") + what + ": '" + key
                                 + "' missing or not an integer");
    return *v;
}

opinion_record table_to_opinion(const sol::table& t)
{
    opinion_record op;
    op.p = require_string(t, "p", "opinion");
    op.m = require_string(t, "m", "opinion");
    // 'on' may be authored as a string or a number (bare subject id); normalise.
    sol::object on_obj = t.get<sol::object>("on");
    if (on_obj.is<std::string>())
        op.on = on_obj.as<std::string>();
    else if (on_obj.is<double>())
        op.on = std::to_string(static_cast<long long>(on_obj.as<double>()));
    else
        throw std::runtime_error("persona pack opinion: 'on' missing");
    op.c = require_string(t, "c", "opinion");
    op.w = require_int(t, "w", "opinion");
    op.v = opt_string(t, "v");
    return op;
}

verdict_record table_to_verdict(const sol::table& t)
{
    verdict_record v;
    v.v        = require_string(t, "v", "verdict");
    sol::object on_obj = t.get<sol::object>("on");
    v.on = on_obj.is<std::string>() ? on_obj.as<std::string>()
         : on_obj.is<double>()     ? std::to_string(static_cast<long long>(on_obj.as<double>()))
                                    : std::string();
    v.baseline = opt_string(t, "baseline");
    v.c        = require_string(t, "c", "verdict");
    return v;
}

sol::table opinion_to_table(sol::state& lua, const opinion_record& op)
{
    sol::table t = lua.create_table();
    t["p"] = op.p;
    t["m"] = op.m;
    t["on"] = op.on;
    t["c"] = op.c;
    t["w"] = op.w;
    if (!op.v.empty())
        t["v"] = op.v;
    return t;
}

} // namespace

// ---------------------------------------------------------------------------
// pack::impl — owns the sandboxed sol2 state. Kept out of the header so no
// other translation unit needs <sol/sol.hpp> to see a persona::pack.
// ---------------------------------------------------------------------------
struct pack::impl
{
    // base/math/string/table only — the pack authoring rule (table.sort etc.
    // over findings, per the seed packs) plus lua_state's own restriction: no
    // io/os/package, no require. A persona pack cannot touch the filesystem,
    // the clock, or load another module. It never receives a world pointer
    // either; the only input is the plain fact table this loader marshals in.
    sol::state lua;
    sol::protected_function extract_fn;
    sol::protected_function policy_fn;
    sol::protected_function phrase_fn;
    sol::protected_function aggregate_fn; // verdict bench only
};

pack::pack(const std::string& lua_path)
    : m_impl(std::make_unique<impl>())
{
    m_impl->lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

    sol::protected_function_result result =
        m_impl->lua.safe_script_file(lua_path, sol::script_pass_on_error);
    if (!result.valid())
    {
        sol::error err = result;
        throw std::runtime_error("persona pack '" + lua_path + "': " + err.what());
    }

    // .valid() only proves no error was raised; the return shape is unproven.
    if (result.get_type() != sol::type::table)
        throw std::runtime_error("persona pack '" + lua_path + "': script must return a table");
    sol::table t = result;
    m_id                = require_string(t, "id", lua_path.c_str());
    m_bench             = require_string(t, "bench", lua_path.c_str());
    m_seal              = require_string(t, "seal", lua_path.c_str());
    m_budget            = require_int(t, "budget", lua_path.c_str());
    m_failure_condition = require_string(t, "failure_condition", lua_path.c_str());

    static const char* const kBenches[] = { "hearth", "banner", "mountain", "verdict" };
    if (std::none_of(std::begin(kBenches), std::end(kBenches),
                     [&](const char* b) { return m_bench == b; }))
        throw std::runtime_error("persona pack '" + lua_path + "': bench '" + m_bench
                                 + "' not one of hearth/banner/mountain/verdict");

    m_impl->extract_fn = t.get<sol::protected_function>("extract");
    m_impl->policy_fn  = t.get<sol::protected_function>("policy");
    m_impl->phrase_fn  = t.get<sol::protected_function>("phrase");
    if (!m_impl->extract_fn.valid() || !m_impl->policy_fn.valid() || !m_impl->phrase_fn.valid())
        throw std::runtime_error("persona pack '" + lua_path
                                 + "': extract/policy/phrase must all be functions");

    if (m_bench == "verdict")
    {
        m_impl->aggregate_fn = t.get<sol::protected_function>("aggregate");
        if (!m_impl->aggregate_fn.valid())
            throw std::runtime_error("persona pack '" + lua_path
                                     + "': verdict bench requires aggregate()");
    }
}

pack::~pack()                              = default;
pack::pack(pack&&) noexcept                = default;
pack& pack::operator=(pack&&) noexcept     = default;

std::vector<opinion_record> pack::evaluate(const corp_blackboard& bb) const
{
    sol::state& lua = m_impl->lua;

    // Fixed per-eval budget enforced HERE, on the C++ side — never trusted to
    // the pack. Facts beyond budget() are never marshalled into Lua at all.
    const std::size_t n = std::min(bb.facts.size(), static_cast<std::size_t>(std::max(0, m_budget)));

    sol::table facts = lua.create_table(static_cast<int>(n), 0);
    for (std::size_t i = 0; i < n; ++i)
    {
        const corp_fact&  f         = bb.facts[i];
        const std::string predicate = bridge_predicate(f.predicate);
        sol::table ft = lua.create_table();
        ft["t"]          = f.tick;
        ft["subject"]    = f.subject;
        ft["predicate"]  = predicate;
        if (predicate == "body.activity")
            ft["value"] = bridge_activity_tier(f.value); // string tier, not the raw enum number
        else
            ft["value"] = f.value;
        ft["confidence"] = f.confidence;
        ft["provenance"] = provenance_token(f.provenance);
        facts[i + 1] = ft;
    }

    sol::protected_function_result extract_res = m_impl->extract_fn(facts);
    if (!extract_res.valid())
    {
        sol::error err = extract_res;
        throw std::runtime_error("persona pack '" + m_id + "'.extract: " + err.what());
    }
    if (extract_res.get_type() != sol::type::table)
        throw std::runtime_error("persona pack '" + m_id + "'.extract: must return a table");
    sol::table findings = extract_res;

    sol::protected_function_result policy_res = m_impl->policy_fn(findings);
    if (!policy_res.valid())
    {
        sol::error err = policy_res;
        throw std::runtime_error("persona pack '" + m_id + "'.policy: " + err.what());
    }
    if (policy_res.get_type() != sol::type::table)
        throw std::runtime_error("persona pack '" + m_id + "'.policy: must return a table");
    sol::table opinions = policy_res;

    std::vector<opinion_record> out;
    const std::size_t count = opinions.size();
    out.reserve(count);
    for (std::size_t i = 1; i <= count; ++i)
    {
        sol::optional<sol::table> row = opinions.get<sol::optional<sol::table>>(i);
        if (!row)
            throw std::runtime_error("persona pack '" + m_id + "'.policy: opinion "
                                     + std::to_string(i) + " is not a table");
        out.push_back(table_to_opinion(*row));
    }
    return out;
}

std::string pack::phrase_for(const opinion_record& op) const
{
    sol::table t = opinion_to_table(m_impl->lua, op);
    sol::protected_function_result res = m_impl->phrase_fn(t);
    if (!res.valid())
    {
        sol::error err = res;
        throw std::runtime_error("persona pack '" + m_id + "'.phrase: " + err.what());
    }
    sol::optional<std::string> phrase = res.get<sol::optional<std::string>>();
    if (!phrase)
        throw std::runtime_error("persona pack '" + m_id + "'.phrase: must return a string");
    return *phrase;
}

std::vector<verdict_record> pack::aggregate(const std::vector<opinion_record>& opinions) const
{
    if (!is_verdict_bench())
        throw std::runtime_error("persona pack '" + m_id + "': aggregate() called on a non-verdict bench");

    sol::state& lua = m_impl->lua;
    sol::table t = lua.create_table(static_cast<int>(opinions.size()), 0);
    for (std::size_t i = 0; i < opinions.size(); ++i)
        t[i + 1] = opinion_to_table(lua, opinions[i]);

    sol::protected_function_result res = m_impl->aggregate_fn(t);
    if (!res.valid())
    {
        sol::error err = res;
        throw std::runtime_error("persona pack '" + m_id + "'.aggregate: " + err.what());
    }
    if (res.get_type() != sol::type::table)
        throw std::runtime_error("persona pack '" + m_id + "'.aggregate: must return a table");
    sol::table verdicts = res;

    std::vector<verdict_record> out;
    const std::size_t count = verdicts.size();
    out.reserve(count);
    for (std::size_t i = 1; i <= count; ++i)
    {
        sol::optional<sol::table> row = verdicts.get<sol::optional<sol::table>>(i);
        if (!row)
            throw std::runtime_error("persona pack '" + m_id + "'.aggregate: verdict "
                                     + std::to_string(i) + " is not a table");
        out.push_back(table_to_verdict(*row));
    }
    return out;
}

std::vector<pack> load_bench(const std::string& dir)
{
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file())
            continue;
        const std::filesystem::path& p = entry.path();
        if (p.extension() != ".lua")
            continue;
        if (p.filename() == "pack_schema.lua")
            continue; // the validator itself, not a pack
        files.push_back(p);
    }
    std::sort(files.begin(), files.end()); // deterministic load order

    std::vector<pack> packs;
    packs.reserve(files.size());
    for (const auto& f : files)
        packs.emplace_back(f.string());
    return packs;
}

} // namespace persona
