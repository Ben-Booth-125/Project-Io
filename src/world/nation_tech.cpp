#include "nation_tech.hpp"

#include "campaign_settle.hpp" // k_campaign_settle_ticks (BL-1324: the age quarter)
#include "components.hpp"
#include "nation_budget.hpp"
#include "recipe_registry.hpp"
#include "resource_names.hpp"
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

bool valid_node(int node)
{
    return node >= 0 && node < io::dg::node_count;
}

const io::dg::node& node_at(int node)
{
    return io::dg::nodes[static_cast<std::size_t>(node)];
}

const nation_tech_state* state_of(const world& w, entity_id nation)
{
    const auto it = w.nation_tech.find(nation);
    return it == w.nation_tech.end() ? nullptr : &it->second;
}

bool in_list(const std::vector<std::uint16_t>& v, int node)
{
    return std::find(v.begin(), v.end(), static_cast<std::uint16_t>(node)) != v.end();
}

bool groups_met(const std::bitset<io::dg::node_count>& held, int node)
{
    const io::dg::node& n = node_at(node);
    for (int g = 0; g < n.group_count; ++g)
    {
        bool any = false;
        for (const std::int16_t m : n.groups[static_cast<std::size_t>(g)])
            if (m >= 0 && held.test(static_cast<std::size_t>(m)))
            {
                any = true;
                break;
            }
        if (!any)
            return false;
    }
    return true;
}

// The enqueue planner: a pure expansion over (held, queue, plan) that never
// touches the world. `enqueue_node` commits the plan only when the whole
// expansion succeeded, so a refusal mutates nothing.
struct planner
{
    const std::bitset<io::dg::node_count>& held;
    const std::vector<std::uint16_t>&      queue;
    std::vector<std::uint16_t>             plan; // new entries, in queue order
    research_params                        params;

    bool present(int n) const
    {
        return held.test(static_cast<std::size_t>(n)) || in_list(queue, n) || in_list(plan, n);
    }

    bool pair_blocked(int n) const
    {
        const int ex = node_at(n).excludes;
        return ex >= 0 && present(ex);
    }

    std::int64_t plan_cost() const
    {
        std::int64_t c = 0;
        for (const std::uint16_t n : plan)
            c += node_cost_milli(n, params);
        return c;
    }

    bool expand(int n, int depth)
    {
        if (depth > io::dg::node_count)
            return false; // the store is a DAG (tree_lint); this only bounds a corrupt one
        if (present(n))
            return true;
        if (pair_blocked(n))
            return false;
        const io::dg::node& nd = node_at(n);
        for (int g = 0; g < nd.group_count; ++g)
        {
            const auto& grp = nd.groups[static_cast<std::size_t>(g)];
            bool satisfied = false;
            for (const std::int16_t m : grp)
                if (m >= 0 && present(m))
                {
                    satisfied = true;
                    break;
                }
            if (satisfied)
                continue;

            // A choice: the member that is cheapest to REACH from here, its own
            // unheld path included; ties go to the earlier member (store order).
            bool        found     = false;
            std::int64_t best_cost = std::numeric_limits<std::int64_t>::max();
            std::vector<std::uint16_t> best_plan;
            for (const std::int16_t m : grp)
            {
                if (m < 0)
                    continue;
                planner trial{held, queue, plan, params};
                if (!trial.expand(m, depth + 1))
                    continue;
                const std::int64_t c = trial.plan_cost();
                if (!found || c < best_cost)
                {
                    found     = true;
                    best_cost = c;
                    best_plan = std::move(trial.plan);
                }
            }
            if (!found)
                return false;
            plan = std::move(best_plan);
        }
        // A requirement's expansion could have queued this node's pair sibling.
        if (pair_blocked(n))
            return false;
        plan.push_back(static_cast<std::uint16_t>(n));
        return true;
    }
};

} // namespace

namespace {
// Every milli-RP quantity saturates here: far past any real pool, far short
// of int64's edge, so a sum of a few of them cannot overflow either.
constexpr std::int64_t k_milli_cap = 4'000'000'000'000'000'000LL;

std::int64_t to_milli(double rp)
{
    if (!std::isfinite(rp) || rp <= 0.0)
        return 0;
    const double m = rp * static_cast<double>(k_milli_per_rp);
    if (m >= static_cast<double>(k_milli_cap))
        return k_milli_cap;
    return static_cast<std::int64_t>(std::llround(m));
}

/// a + b for non-negative a, b, saturating at k_milli_cap (BL-1324 review).
std::int64_t sat_add(std::int64_t a, std::int64_t b)
{
    a = std::clamp<std::int64_t>(a, 0, k_milli_cap);
    b = std::clamp<std::int64_t>(b, 0, k_milli_cap);
    return a > k_milli_cap - b ? k_milli_cap : a + b;
}

/// llround(m x mult) for a milli-RP m and the research multiplier, saturating
/// at k_milli_cap — the pass's own two-step rounding, kept bit for bit.
std::int64_t scale_milli(std::int64_t m, double mult)
{
    const double v = static_cast<double>(m) * mult;
    if (!std::isfinite(v) || v <= 0.0)
        return 0;
    if (v >= static_cast<double>(k_milli_cap))
        return k_milli_cap;
    return static_cast<std::int64_t>(std::llround(v));
}
} // namespace

namespace {
constexpr std::int64_t k_price_unit_ppm = 1'000'000;
constexpr std::int64_t k_price_cap_ppm  = 1'000'000'000'000LL; // a millionfold: overflow guard only

/// The quarterly rate the growth dial compounds at, integer ppm (0 = flat).
std::int64_t price_rate_ppm(const research_params& p)
{
    const double g = static_cast<double>(p.price_growth_per_year);
    if (!std::isfinite(g) || g <= 0.0)
        return 0;
    return std::llround(std::min(g, 10.0) * 1.0e6 / 4.0);
}

/// One quarter of compounding: the index plus rate/4 of it, integer, capped.
/// The ONE step both the index at a quarter and the projection's walk take.
std::int64_t compound_one(std::int64_t idx, std::int64_t rate_ppm)
{
    if (rate_ppm <= 0 || idx >= k_price_cap_ppm)
        return std::min(idx, k_price_cap_ppm);
    idx += (idx / k_price_unit_ppm) * rate_ppm
         + ((idx % k_price_unit_ppm) * rate_ppm + k_price_unit_ppm / 2) / k_price_unit_ppm;
    return std::min(idx, k_price_cap_ppm);
}
} // namespace

std::int64_t node_cost_milli(int node, const research_params& p, std::int64_t price_ppm)
{
    if (!valid_node(node))
        return 0;
    const std::int64_t unpriced = to_milli(static_cast<double>(node_at(node).cost_units)
                                           * static_cast<double>(p.rp_per_cost_unit));
    // BL-1324: the price index, in integer arithmetic. At the opening price
    // (1'000'000 ppm) this is the unpriced cost exactly, so a flat-price world
    // charges what it always did.
    if (price_ppm == k_price_unit_ppm)
        return unpriced;
    const std::int64_t idx   = std::clamp<std::int64_t>(price_ppm, 0, k_price_cap_ppm);
    const std::int64_t whole = idx / k_price_unit_ppm;
    const std::int64_t frac  = idx % k_price_unit_ppm;
    // unpriced x whole, saturating.
    if (whole > 0 && unpriced > k_milli_cap / whole)
        return k_milli_cap;
    const std::int64_t whole_part = unpriced * whole;
    // unpriced x frac / 1e6 without forming unpriced x frac: split unpriced into
    // hi x 1e6 + lo, so hi x frac <= unpriced and lo x frac < 1e12. Exactly the
    // rounded (unpriced x frac + 5e5) / 1e6.
    const std::int64_t hi = unpriced / k_price_unit_ppm;
    const std::int64_t lo = unpriced % k_price_unit_ppm;
    const std::int64_t frac_part = hi * frac + (lo * frac + k_price_unit_ppm / 2) / k_price_unit_ppm;
    return sat_add(whole_part, frac_part);
}

std::int64_t research_price_ppm_at(int age_quarter, const research_params& p)
{
    // A nominal annual rate compounded quarterly: each quarter adds rate/4 of
    // the index. The dial becomes an integer ppm rate ONCE; the compounding is
    // integer, so the index is the same on every platform.
    const std::int64_t rate = price_rate_ppm(p);
    std::int64_t idx = k_price_unit_ppm;
    for (int q = 0; q < age_quarter && rate > 0 && idx < k_price_cap_ppm; ++q)
        idx = compound_one(idx, rate);
    return idx;
}

int research_age_quarter(const world& w)
{
    return std::max(0, w.current_econ_tick - k_campaign_settle_ticks);
}

int research_next_age_quarter(const world& w)
{
    // Between passes `current_econ_tick` is the step the LAST pass ran at, in a
    // fresh campaign and after a load alike (`econ_tick_after_live_steps`), so
    // the next pass runs at one more.
    return std::max(0, w.current_econ_tick + 1 - k_campaign_settle_ticks);
}

bool research_age_open(const world& w)
{
    return w.current_econ_tick >= k_campaign_settle_ticks;
}

std::int64_t research_price_ppm(const world& w, const research_params& p)
{
    return research_price_ppm_at(research_age_quarter(w), p);
}

namespace {
// BL-1402 (NATIONS.md § What a nation owns): any economic owner's contribution.
// A corporation's is its previous filed return (above). A nation holding
// buildings files no return; its term is the sales its own buildings made
// this tick (`state_ledger` income, which clear_markets books before the
// nation step reads it), never negative. One economy tick is one quarter.
std::int64_t owner_economy_milli(const world& w, entity_id owner)
{
    // BL-1407: every owner is a nation (the corporation's previous-return term
    // is deleted with the type).
    if (w.nations.find(owner) == w.nations.end())
        return 0;
    const auto lit = w.state_ledger.find(owner);
    if (lit == w.state_ledger.end())
        return 0;
    const double inc = static_cast<double>(lit->second.income);
    if (!std::isfinite(inc) || inc <= 0.0)
        return 0;
    return to_milli(inc);
}
} // namespace

std::int64_t nation_economy_milli(const world& w, entity_id nation)
{
    if (nation == null_entity)
        return 0;
    std::int64_t sum = 0;
    for (const entity_id owner : economic_owner_ids(w))
        if (owner_home_nation(w, owner) == nation)
            sum = sat_add(sum, owner_economy_milli(w, owner));
    return sum;
}

std::int64_t line_rp_milli(float credits, const research_params& p)
{
    if (!(credits > 0.0f) || !std::isfinite(credits))
        return 0;
    const double c = static_cast<double>(credits);
    return to_milli(static_cast<double>(p.rp_per_research_credit) * c
                    + static_cast<double>(p.rp_per_research_credit_sqrt) * std::sqrt(c));
}

double line_credits_for(std::int64_t milli, const research_params& p)
{
    if (milli <= 0)
        return 0.0;
    const double need = static_cast<double>(milli) / static_cast<double>(k_milli_per_rp);
    const double a = std::max(0.0, static_cast<double>(p.rp_per_research_credit));
    const double b = std::max(0.0, static_cast<double>(p.rp_per_research_credit_sqrt));
    // a*c + b*sqrt(c) = need, with s = sqrt(c): a*s^2 + b*s - need = 0.
    double s = 0.0;
    if (a > 0.0)
        s = (-b + std::sqrt(b * b + 4.0 * a * need)) / (2.0 * a);
    else if (b > 0.0)
        s = need / b;
    else
        return std::numeric_limits<double>::infinity();
    return s * s;
}

bool nation_holds(const world& w, entity_id nation, int node)
{
    if (!valid_node(node))
        return false;
    const nation_tech_state* st = state_of(w, nation);
    return st != nullptr && st->held.test(static_cast<std::size_t>(node));
}

bool corp_nation_holds(const world& w, entity_id corp, int node)
{
    // BL-1399: the actor may be a nation itself (NATIONS.md § What a nation owns);
    // owner_home_nation resolves either kind to the nation whose tree it reads.
    const entity_id nation = owner_home_nation(w, corp);
    if (nation == null_entity)
        return false;
    return nation_holds(w, nation, node);
}

bool node_requirements_met(const world& w, entity_id nation, int node)
{
    if (!valid_node(node))
        return false;
    const nation_tech_state* st = state_of(w, nation);
    if (st == nullptr)
        return node_at(node).group_count == 0;
    return groups_met(st->held, node);
}

bool deed_met(const world& /*w*/, entity_id /*nation*/, int node)
{
    if (!valid_node(node))
        return false;
    const io::dg::node& n = node_at(node);
    if (!n.is_quest)
        return true;
    // Every Digitisation deed is declared unread (`deed_waits_on`): each reads
    // a quantity the campaign does not carry (tech_tree_store.js holds the
    // store to that). A deed given a machine form is evaluated HERE, against
    // the nation; until one is, a quest waits in the queue.
    return false;
}

enqueue_result enqueue_node(world& w, entity_id nation, int node)
{
    if (!valid_node(node) || w.nations.find(nation) == w.nations.end())
        return enqueue_result::invalid;

    const nation_tech_state* existing = state_of(w, nation);
    static const nation_tech_state empty{};
    const nation_tech_state& st = existing != nullptr ? *existing : empty;

    if (st.held.test(static_cast<std::size_t>(node)))
        return enqueue_result::already_held;
    if (in_list(st.queue, node))
        return enqueue_result::already_queued;

    planner pl{st.held, st.queue, {}, research_params{}};
    if (!pl.expand(node, 0))
        return enqueue_result::refused_pair;

    nation_tech_state& dst = w.nation_tech[nation];
    for (const std::uint16_t n : pl.plan)
        dst.queue.push_back(n);
    return enqueue_result::queued;
}

// ---------------------------------------------------------------------------
// Node effects (BL-1274)
// ---------------------------------------------------------------------------

namespace {

// THE CODE'S DECLARATION. Each list below is what this file (and the read
// sites it serves) actually consumes; nation_tech_harness R1 holds the store
// to it. Adding a reader means adding it here in the same change. An effect
// has two states (RESEARCH.md): read, or declared unread — no third.

/// Modifier subjects with a live consumer: extraction_rate (extraction_nominal),
/// processing_yield (credit_processing_outputs), logistics_cost
/// (price_convoy_leg), national_research (advance_research). unit_upkeep and
/// wage_floor have no read site that carries a nation (resolve_unit_upkeep and
/// compute_building_opex take none); collapse_strain has no campaign accumulator.
constexpr modifier_subject k_read_subjects[] = {
    modifier_subject::extraction_rate, modifier_subject::processing_yield,
    modifier_subject::logistics_cost, modifier_subject::national_research,
};

/// Mechanic keys with a reader. `space_dispatch` (Rocketry, Ben 2026-10-10):
/// read by price_convoy_leg's inter-body branch through `nation_mechanic`.
constexpr std::string_view k_read_mechanics[] = { "space_dispatch" };

/// Named extraction sites a scope may name (`extraction_site_name`).
constexpr std::string_view k_sites[] = { "farm", "fishing_wharf", "well" };

/// The systems an unread effect or deed may wait on. Each is a lever the
/// campaign does not carry (or carries without a nation to read it by).
constexpr std::string_view k_unread_systems[] = {
    "Alarm", "Ceiling", "Container Port", "Marketplace trade capacity", "Nuclear Plant",
    "Port throughput", "activity fog", "assimilation", "battle resolution",
    "building fuel draw", "building maintenance by nation", "coastal water placement",
    "construction cost by nation", "convoy cargo capacity", "deed history", "espionage",
    "fertiliser recipe", "holding losses by nation", "household demand by nation",
    "launch draw by nation", "microchip recipe", "muster time",
    "national stock and demand ledger", "offshore placement", "plastics recipe", "politics",
    "port handling by nation", "province building ceiling by type", "roads", "sentiment",
    "survey reach", "tariffs", "tax yield", "trade partners by nation", "trade points",
    "trade settlement timing", "travel time by nation", "unit movement", "unit strength",
    "unit unlock", "unit_upkeep", "unrest", "wage_floor", "war outcomes",
};

bool parse_subject(std::string_view s, modifier_subject& out)
{
    static constexpr std::pair<std::string_view, modifier_subject> table[] = {
        { "extraction_rate", modifier_subject::extraction_rate },
        { "processing_yield", modifier_subject::processing_yield },
        { "unit_upkeep", modifier_subject::unit_upkeep },
        { "logistics_cost", modifier_subject::logistics_cost },
        { "wage_floor", modifier_subject::wage_floor },
        { "collapse_strain", modifier_subject::collapse_strain },
        { "national_research", modifier_subject::national_research },
    };
    for (const auto& [name, v] : table)
        if (name == s)
        {
            out = v;
            return true;
        }
    return false;
}

bool parse_building(std::string_view s, building_type& out)
{
    // The scripts/economy.lua keys (recipe_registry.cpp's own table).
    static constexpr std::pair<std::string_view, building_type> table[] = {
        { "extraction_site", building_type::extraction_site },
        { "processing_facility", building_type::processing_facility },
        { "port", building_type::port },
        { "launchpad", building_type::launchpad },
        { "inland_logistics_hub", building_type::inland_logistics_hub },
        { "military_base", building_type::military_base },
        { "airbase", building_type::airbase }, // BL-1449
        { "research_institute", building_type::research_institute },
        { "schooling", building_type::schooling },
        { "university", building_type::university },
    };
    for (const auto& [name, v] : table)
        if (name == s)
        {
            out = v;
            return true;
        }
    return false;
}

template <std::size_t N>
bool in(const std::string_view (&list)[N], std::string_view s)
{
    return !s.empty() && std::find(std::begin(list), std::end(list), s) != std::end(list);
}

enum class scope_kind : std::uint8_t { all, resource, group, recipe, building, site, bad };

struct resolved_modifier
{
    std::size_t      node     = 0;
    modifier_subject subject  = modifier_subject::extraction_rate;
    scope_kind       scope    = scope_kind::bad;
    int              resource = -1;
    building_type    building = building_type::none;
    std::string_view text;  // the group, recipe or site name
    scalar_modifier  mod;
};

struct resolved_unlock
{
    std::size_t      node = 0;
    std::string_view recipe;                         // unlock_recipe
    building_type    building = building_type::none; // unlock_structure
};

struct resolved_mechanic
{
    std::size_t      node = 0;
    std::string_view key;
};

constexpr std::string_view k_resource_prefix = "resource:";
constexpr std::string_view k_group_prefix    = "group:";
constexpr std::string_view k_recipe_prefix   = "recipe:";
constexpr std::string_view k_building_prefix = "building:";
constexpr std::string_view k_site_prefix     = "site:";

void parse_scope(std::string_view s, resolved_modifier& r)
{
    auto starts = [&](std::string_view p) { return s.substr(0, p.size()) == p; };
    if (s == "all")
        r.scope = scope_kind::all;
    else if (starts(k_resource_prefix))
    {
        bool ok = false;
        const resource_type t = resource_names::resource_from_name(
            std::string(s.substr(k_resource_prefix.size())), ok);
        if (ok)
        {
            r.scope    = scope_kind::resource;
            r.resource = static_cast<int>(t);
        }
    }
    else if (starts(k_group_prefix) && s.size() > k_group_prefix.size())
    {
        r.scope = scope_kind::group;
        r.text  = s.substr(k_group_prefix.size());
    }
    else if (starts(k_recipe_prefix) && s.size() > k_recipe_prefix.size())
    {
        r.scope = scope_kind::recipe;
        r.text  = s.substr(k_recipe_prefix.size());
    }
    else if (starts(k_building_prefix) && parse_building(s.substr(k_building_prefix.size()), r.building))
        r.scope = scope_kind::building;
    else if (starts(k_site_prefix) && in(k_sites, s.substr(k_site_prefix.size())))
    {
        r.scope = scope_kind::site;
        r.text  = s.substr(k_site_prefix.size());
    }
}

struct effect_tables
{
    // Read modifiers, per subject, in node-index order (the effects table's own order).
    std::vector<resolved_modifier> by_subject[static_cast<std::size_t>(modifier_subject::national_research) + 1];
    std::vector<resolved_unlock>   recipe_unlocks;
    std::vector<resolved_unlock>   structure_unlocks;
    std::vector<resolved_mechanic> mechanics;
};

const effect_tables& tables()
{
    // Built once from the constexpr store table: a pure function of the
    // header, so every process holds the same rows in the same order.
    static const effect_tables t = [] {
        effect_tables out;
        for (const io::dg::effect& e : io::dg::effects)
        {
            if (classify_effect(e) != effect_reading::read)
                continue;
            const std::size_t node = static_cast<std::size_t>(e.node);
            if (e.kind == io::dg::effect_kind::modifier)
            {
                resolved_modifier r;
                r.node = node;
                parse_subject(e.subject, r.subject);
                parse_scope(e.scope, r);
                r.mod.subject   = r.subject;
                r.mod.op        = e.op == io::dg::effect_op::add ? modifier_op::add : modifier_op::multiply;
                r.mod.magnitude = e.value;
                out.by_subject[static_cast<std::size_t>(r.subject)].push_back(r);
            }
            else if (e.kind == io::dg::effect_kind::unlock_recipe)
                out.recipe_unlocks.push_back({ node, e.target, building_type::none });
            else if (e.kind == io::dg::effect_kind::unlock_structure)
            {
                resolved_unlock u{ node, {}, building_type::none };
                parse_building(e.target, u.building);
                out.structure_unlocks.push_back(u);
            }
            else if (e.kind == io::dg::effect_kind::mechanic)
                out.mechanics.push_back({ node, e.target });
        }
        return out;
    }();
    return t;
}

bool scope_matches(const resolved_modifier& r, const effect_target& t)
{
    switch (r.scope)
    {
        case scope_kind::all:      return !t.scoped_only;
        case scope_kind::resource: return t.resource >= 0 && r.resource == t.resource;
        case scope_kind::group:    return !t.group.empty() && r.text == t.group;
        case scope_kind::recipe:   return !t.recipe.empty() && r.text == t.recipe;
        case scope_kind::building: return t.building != building_type::none && r.building == t.building;
        case scope_kind::site:     return !t.site.empty() && r.text == t.site;
        case scope_kind::bad:      return false;
    }
    return false;
}

} // namespace

std::string_view extraction_site_name(resource_type target, bool depositless)
{
    if (target == resource_type::agricultural_produce)
        return depositless ? "fishing_wharf" : "farm";
    if (target == resource_type::water && depositless)
        return "well";
    return {};
}

bool modifier_subject_read(modifier_subject subject)
{
    return std::find(std::begin(k_read_subjects), std::end(k_read_subjects), subject)
           != std::end(k_read_subjects);
}

const std::vector<std::string_view>& declared_unread_systems()
{
    static const std::vector<std::string_view> v(std::begin(k_unread_systems), std::end(k_unread_systems));
    return v;
}

effect_reading classify_effect(const io::dg::effect& e, const recipe_registry* reg)
{
    const std::string_view target = e.target;
    switch (e.kind)
    {
        case io::dg::effect_kind::modifier:
        {
            modifier_subject s{};
            if (!parse_subject(e.subject, s) || !modifier_subject_read(s))
                return effect_reading::invalid;
            resolved_modifier r;
            parse_scope(e.scope, r);
            if (r.scope == scope_kind::bad || !std::isfinite(e.value))
                return effect_reading::invalid;
            if (reg != nullptr && (r.scope == scope_kind::group || r.scope == scope_kind::recipe))
            {
                bool found = false;
                for (std::size_t i = 0; i < reg->recipe_count() && !found; ++i)
                    if (const recipe* rc = reg->get_recipe(static_cast<std::uint16_t>(i)))
                        found = r.scope == scope_kind::group ? rc->group == r.text : rc->name == r.text;
                if (!found)
                    return effect_reading::invalid;
            }
            return effect_reading::read;
        }
        case io::dg::effect_kind::unlock_recipe:
            if (target.empty())
                return effect_reading::invalid;
            if (reg != nullptr && reg->recipe_id(std::string(target)) == no_recipe)
                return effect_reading::invalid; // a recipe the registry lacks is `unread`
            return effect_reading::read;
        case io::dg::effect_kind::unlock_structure:
        {
            building_type b{};
            return parse_building(target, b) ? effect_reading::read : effect_reading::invalid;
        }
        case io::dg::effect_kind::mechanic:
            return in(k_read_mechanics, target) ? effect_reading::read : effect_reading::invalid;
        case io::dg::effect_kind::unread:
            if (e.what[0] == '\0' || !in(k_unread_systems, e.waits_on))
                return effect_reading::invalid;
            return effect_reading::unread;
    }
    return effect_reading::invalid;
}

float nation_scalar(const world& w, entity_id nation, modifier_subject subject,
                    const effect_target& target, float base)
{
    const nation_tech_state* st = state_of(w, nation);
    if (st == nullptr || st->held.none())
        return base; // holding nothing: the same bits back, no arithmetic
    const std::size_t si = static_cast<std::size_t>(subject);
    const effect_tables& t = tables();
    if (si >= std::size(t.by_subject))
        return base;
    // Node-index order: the held set has no earn order, and needs none saved.
    for (const resolved_modifier& r : t.by_subject[si])
        if (st->held.test(r.node) && scope_matches(r, target))
            base = apply_scalar_modifier(base, r.mod);
    return base;
}

float node_scalar_at(int node, modifier_subject subject, const effect_target& target, float base)
{
    if (!valid_node(node))
        return base;
    const std::size_t si = static_cast<std::size_t>(subject);
    const effect_tables& t = tables();
    if (si >= std::size(t.by_subject))
        return base;
    // The fold nation_scalar runs, over this one node's read rows (its effect order).
    for (const resolved_modifier& r : t.by_subject[si])
        if (r.node == static_cast<std::size_t>(node) && scope_matches(r, target))
            base = apply_scalar_modifier(base, r.mod);
    return base;
}

bool node_has_mechanic(int node, std::string_view key)
{
    for (const resolved_mechanic& m : tables().mechanics)
        if (m.node == static_cast<std::size_t>(node) && m.key == key)
            return true;
    return false;
}

float research_multiplier(const world& w, entity_id nation)
{
    // 1 + the held "+N% national research" shares (scope "all", op add).
    return nation_scalar(w, nation, modifier_subject::national_research, effect_target{}, 1.0f);
}

bool nation_mechanic(const world& w, entity_id nation, std::string_view key)
{
    bool named = false;
    for (const resolved_mechanic& m : tables().mechanics)
    {
        if (m.key != key)
            continue;
        named = true;
        if (nation_holds(w, nation, static_cast<int>(m.node)))
            return true;
    }
    return !named;
}

bool structure_unlocked(const world& w, entity_id corp, building_type type)
{
    const effect_tables& t = tables();
    if (t.structure_unlocks.empty() || corp == null_entity)
        return true;
    bool named = false;
    for (const resolved_unlock& u : t.structure_unlocks)
    {
        if (u.building != type)
            continue;
        named = true;
        if (corp_nation_holds(w, corp, static_cast<int>(u.node)))
            return true;
    }
    return !named;
}

bool recipe_unlocked(const world& w, const recipe_registry& reg, entity_id corp,
                     std::uint16_t recipe_id)
{
    const effect_tables& t = tables();
    if (t.recipe_unlocks.empty() || corp == null_entity)
        return true;
    const recipe* rc = reg.get_recipe(recipe_id);
    if (rc == nullptr)
        return true;
    // A recipe one or more nodes unlock is open once the corp's home nation
    // holds ANY of them; a recipe no node names is open.
    bool named = false;
    for (const resolved_unlock& u : t.recipe_unlocks)
    {
        if (u.recipe != rc->name)
            continue;
        named = true;
        if (corp_nation_holds(w, corp, static_cast<int>(u.node)))
            return true;
    }
    return !named;
}

namespace {

// The queue half of the quarterly pass over one nation's state. Shared by
// `flow_research_queue` (the live state) and `project_queue_arrivals` (a copy):
// one loop, so the projection cannot disagree with what the pass will do.
int flow_state(const world& w, entity_id nation, nation_tech_state& st, const research_params& p,
               std::int64_t price_ppm)
{
    int completed = 0;
    bool moved = true;
    while (moved)
    {
        moved = false;
        for (std::size_t i = 0; i < st.queue.size(); ++i)
        {
            const int n = st.queue[i];
            const int ex = node_at(n).excludes;
            if (st.held.test(static_cast<std::size_t>(n))
                || (ex >= 0 && st.held.test(static_cast<std::size_t>(ex))))
            {
                // Held already, or its pair sibling is: it can never complete.
                st.queue.erase(st.queue.begin() + static_cast<std::ptrdiff_t>(i));
                moved = true;
                break;
            }
            // Passed over, not blocking: a quest whose deed is unmet, or an
            // entry whose requirement is itself waiting further up.
            if (!groups_met(st.held, n) || !deed_met(w, nation, n))
                continue;

            const std::int64_t cost = node_cost_milli(n, p, price_ppm);
            if (st.pool_milli < cost)
                return completed; // the head waits for funding; RP stay in the pool

            st.pool_milli -= cost;
            st.held.set(static_cast<std::size_t>(n));
            st.queue.erase(st.queue.begin() + static_cast<std::ptrdiff_t>(i));
            ++completed;
            moved = true; // the remainder flows on; rescan (a completion can open a passed entry)
            break;
        }
    }
    return completed;
}

// Universities by the nation whose territory they stand in; research
// institutes by their owner corp's home nation. Counted in integers, so the
// unordered corporation walk cannot reorder a sum.
struct research_buildings
{
    std::map<entity_id, int> universities;
    std::map<entity_id, int> institutes;
    /// BL-1324: `nation_economy_milli` per home nation, summed in integers.
    std::map<entity_id, std::int64_t> economy_milli;
};

research_buildings count_research_buildings(const world& w)
{
    research_buildings rb;
    // BL-1402: every economic owner — corporations and nations holding
    // buildings alike — so a state University or institute counts too.
    for (const entity_id owner : economic_owner_ids(w))
    {
        const entity_id home = owner_home_nation(w, owner);
        if (home != null_entity)
        {
            std::int64_t& e = rb.economy_milli[home];
            e = sat_add(e, owner_economy_milli(w, owner));
        }
        const std::vector<entity_id>* assets = assets_of(w, owner);
        if (assets == nullptr)
            continue;
        for (const entity_id bid : *assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            const building_component& b = bit->second;
            // RESEARCH.md § Research points: each STAFFED University and
            // institute adds RP — completed, standing, and with workforce
            // assigned. Neither type enters the labour claims (economy_system),
            // so `workforce_assigned` is the staffing the building was given:
            // construction staffs both at 0.5 (construction_initial_workforce;
            // Ben, 2026-10-10), so a completed one counts unless unstaffed.
            if (b.ticks_remaining > 0 || b.decommissioned || !(b.workforce_assigned > 0.0f))
                continue;
            if (b.type == building_type::university)
            {
                if (const auto tn = w.tile_to_nation.find(b.tile); tn != w.tile_to_nation.end())
                    ++rb.universities[tn->second];
            }
            else if (b.type == building_type::research_institute)
            {
                if (home != null_entity)
                    ++rb.institutes[home];
            }
        }
    }
    return rb;
}

// One nation's accrual for a quarter, COMPUTED and not applied: the RP it
// would add and what the line would spend of @p treasury. @p st is the state
// the queue's bill is read from (null = holds nothing, owes nothing). The
// quarterly pass applies it; the research panel's projection reads it.
research_accrual compute_accrual(const world& w, entity_id nid, const research_buildings& rb,
                                 const nation_tech_state* st, float treasury,
                                 const research_params& p, std::int64_t price_ppm)
{
    const std::size_t ai = static_cast<std::size_t>(budget_priority::academic_research);
    research_accrual row;
    row.nation = nid;

    if (const auto u = rb.universities.find(nid); u != rb.universities.end())
        row.universities = u->second;
    if (const auto r = rb.institutes.find(nid); r != rb.institutes.end())
        row.institutes = r->second;
    std::int64_t econ_milli = 0;
    if (const auto e = rb.economy_milli.find(nid); e != rb.economy_milli.end())
        econ_milli = e->second;
    row.economy   = static_cast<double>(econ_milli) / 1000.0;
    row.price_ppm = price_ppm;
    row.multiplier = research_multiplier(w, nid);
    const double mult = std::isfinite(row.multiplier) && row.multiplier > 0.0f
                            ? static_cast<double>(row.multiplier) : 0.0;

    // The University factor (BL-1274): held national_research multiplies
    // scoped to the University only (exactly 1 when none is held).
    effect_target uni;
    uni.building    = building_type::university;
    uni.scoped_only = true;
    const double uni_factor = static_cast<double>(
        nation_scalar(w, nid, modifier_subject::national_research, uni, 1.0f));

    // The RP that cost no treasury: the base, the buildings and (BL-1324) the
    // economy term — RP that grow with the nation's economy to meet the prices.
    const std::int64_t free_milli = scale_milli(
        to_milli(static_cast<double>(p.base_rp_per_nation)
                 + static_cast<double>(row.universities) * p.rp_per_university * uni_factor
                 + static_cast<double>(row.institutes) * p.rp_per_institute
                 + static_cast<double>(p.rp_per_economy_unit) * row.economy),
        mult);

    // What the queue still owes: every queued node's cost, less the pool
    // and less what the free RP will bring this quarter.
    std::int64_t owed = 0;
    if (st != nullptr)
    {
        for (const std::uint16_t n : st->queue)
            owed = sat_add(owed, node_cost_milli(n, p, price_ppm));
        // Both sides sit inside +-k_milli_cap (the panel's estimate passes a
        // pool of -1e18), so the difference cannot overflow.
        owed -= std::clamp<std::int64_t>(st->pool_milli, -k_milli_cap, k_milli_cap);
    }
    row.owed_milli = std::max<std::int64_t>(0, owed);
    const std::int64_t line_need = std::max<std::int64_t>(0, owed - free_milli);

    // The line's share: run_national_budget's own formula, recomputed on
    // the run_nation_garrison_upkeep precedent (a line with no corp
    // claimant never reaches that pass's report). It pays min(share, what
    // the queue owes) — the garrison's min(share, bill) — as a direct
    // expenditure, credited to nobody. An empty queue owes nothing and
    // spends nothing.
    if (treasury > 0.0f)
    {
        const auto bit = w.nation_budgets.find(nid);
        if (bit != w.nation_budgets.end())
        {
            const nation_budget& bud = bit->second;
            const float reserve   = std::clamp(bud.reserve_fraction, 0.0f, 1.0f);
            const float spendable = treasury * (1.0f - reserve);
            float weight_total = 0.0f;
            for (std::size_t i = 0; i < priority_count; ++i)
                if (std::isfinite(bud.weights[i]) && bud.weights[i] > 0.0f)
                    weight_total += bud.weights[i];
            if (spendable > 0.0f && weight_total > 0.0f
                && std::isfinite(bud.weights[ai]) && bud.weights[ai] > 0.0f)
                row.line_share = std::min(treasury,
                                          spendable * (bud.weights[ai] / weight_total));
        }
    }
    if (row.line_share > 0.0f && line_need > 0 && mult > 0.0)
    {
        // Clamped before the int64 cast: a small multiplier can lift the
        // quotient past int64's range (the panel's estimate passes a bill no
        // quarter can meet). Inert for every real queue — 2^62 milli-RP.
        const double need_milli = std::min(std::ceil(static_cast<double>(line_need) / mult),
                                           4.0e18);
        const double bill = line_credits_for(static_cast<std::int64_t>(need_milli), p);
        row.line_spend = static_cast<float>(std::min(static_cast<double>(row.line_share), bill));
    }

    const std::int64_t line_milli = scale_milli(line_rp_milli(row.line_spend, p), mult);
    row.rp_milli = sat_add(free_milli, line_milli);
    return row;
}

} // namespace

int flow_research_queue(world& w, entity_id nation, const research_params& p)
{
    const auto it = w.nation_tech.find(nation);
    if (it == w.nation_tech.end())
        return 0;
    return flow_state(w, nation, it->second, p, research_price_ppm(w, p));
}

void clear_research_at_handover(world& w)
{
    w.nation_tech.clear();
}

void advance_research(world& w, const research_params& p, std::vector<research_accrual>* out)
{
    if (w.nations.empty())
        return;

    const research_buildings rb = count_research_buildings(w);
    const std::int64_t price    = research_price_ppm(w, p); // one index for the whole pass

    std::vector<entity_id> ids;
    ids.reserve(w.nations.size());
    for (const auto& kv : w.nations)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());

    for (const entity_id nid : ids)
    {
        nation_component& nc = w.nations.at(nid);
        const auto sit = w.nation_tech.find(nid);
        research_accrual row = compute_accrual(
            w, nid, rb, sit != w.nation_tech.end() ? &sit->second : nullptr, nc.treasury, p, price);
        if (row.line_spend > 0.0f)
            nc.treasury -= row.line_spend;

        // An entry is created only when there is something to hold, so a world
        // where nothing researches keeps an empty map (and its state_hash).
        if (row.rp_milli > 0 || sit != w.nation_tech.end())
        {
            nation_tech_state& st = w.nation_tech[nid];
            st.pool_milli = sat_add(st.pool_milli, row.rp_milli);
            row.completed = flow_state(w, nid, st, p, price);
        }

        if (out != nullptr)
            out->push_back(row);
    }
}

// ---------------------------------------------------------------------------
// Read-only views and the queue edits (BL-1276, the research panel)
// ---------------------------------------------------------------------------

enqueue_result preview_enqueue(const world& w, entity_id nation, int node,
                               std::vector<std::uint16_t>* plan)
{
    if (plan != nullptr)
        plan->clear();
    if (!valid_node(node) || w.nations.find(nation) == w.nations.end())
        return enqueue_result::invalid;

    const nation_tech_state* existing = state_of(w, nation);
    static const nation_tech_state empty{};
    const nation_tech_state& st = existing != nullptr ? *existing : empty;

    if (st.held.test(static_cast<std::size_t>(node)))
        return enqueue_result::already_held;
    if (in_list(st.queue, node))
        return enqueue_result::already_queued;

    planner pl{st.held, st.queue, {}, research_params{}};
    if (!pl.expand(node, 0))
        return enqueue_result::refused_pair;
    if (plan != nullptr)
        *plan = std::move(pl.plan);
    return enqueue_result::queued;
}

std::int64_t estimate_rp_per_quarter_milli(const world& w, entity_id nation,
                                           const research_params& p)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return 0;
    // A bill no quarter can meet, so the line reports its whole share: the
    // rate a nation researches at while its queue owes more than a quarter.
    nation_tech_state owing;
    owing.queue.assign(1, 0);
    owing.pool_milli = -1'000'000'000'000'000LL; // a trillion RP owed: no quarter meets it
    return compute_accrual(w, nation, count_research_buildings(w), &owing,
                           nit->second.treasury, p, research_price_ppm(w, p)).rp_milli;
}

std::vector<int> project_queue_arrivals(const world& w, entity_id nation,
                                        const research_params& p, int horizon_quarters)
{
    const nation_tech_state* existing = state_of(w, nation);
    if (existing == nullptr || existing->queue.empty())
        return {};
    std::vector<int> arrival(existing->queue.size(), -1);
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return arrival;

    nation_tech_state sim = *existing;
    const research_buildings rb = count_research_buildings(w);
    const float treasury = nit->second.treasury; // held: the rate as it stands now
    // BL-1324: the price index RISES through the projection as the passes will
    // charge it. Projected quarter 1 is the NEXT pass (`research_next_age_quarter`:
    // econ step current_econ_tick + 1, which reads 0 straight after the settle
    // and after a load alike), and every later quarter is one more compounding
    // step — O(horizon), the same `compound_one` the index itself takes. The
    // economy is held where it stands, like the treasury.
    const std::int64_t rate = price_rate_ppm(p);
    // Unclamped, so a projection read mid-settle still opens at the right quarter.
    int          age   = w.current_econ_tick + 1 - k_campaign_settle_ticks;
    std::int64_t price = research_price_ppm_at(research_next_age_quarter(w), p);
    for (int q = 1; q <= horizon_quarters && !sim.queue.empty(); ++q)
    {
        if (q > 1)
        {
            ++age; // the age's open charges 1; each quarter after compounds once
            if (age > 0)
                price = compound_one(price, rate);
        }
        const research_accrual row = compute_accrual(w, nation, rb, &sim, treasury, p, price);
        if (row.rp_milli <= 0)
            break;
        sim.pool_milli += row.rp_milli;
        flow_state(w, nation, sim, p, price);
        for (std::size_t i = 0; i < existing->queue.size(); ++i)
            if (arrival[i] < 0 && sim.held.test(existing->queue[i]))
                arrival[i] = q;

        // Every remaining entry passed over (a quest waiting on its deed, or an
        // entry behind one): no later quarter moves the queue either.
        bool any_open = false;
        for (const std::uint16_t n : sim.queue)
            if (groups_met(sim.held, n) && deed_met(w, nation, n))
            {
                any_open = true;
                break;
            }
        if (!any_open)
            break;
    }
    return arrival;
}

bool remove_last_queued(world& w, entity_id nation)
{
    const auto it = w.nation_tech.find(nation);
    if (it == w.nation_tech.end() || it->second.queue.empty())
        return false;
    it->second.queue.pop_back();
    return true;
}

std::vector<std::uint16_t> queued_dependents(const world& w, entity_id nation, int node)
{
    std::vector<std::uint16_t> removed;
    const nation_tech_state* st = state_of(w, nation);
    if (st == nullptr || !valid_node(node))
        return removed;
    const auto at = std::find(st->queue.begin(), st->queue.end(), static_cast<std::uint16_t>(node));
    if (at == st->queue.end())
        return removed;

    // Kept so far: the held set plus every surviving entry ahead in the queue.
    std::bitset<io::dg::node_count> kept = st->held;
    for (auto it = st->queue.begin(); it != at; ++it)
        kept.set(*it);
    std::bitset<io::dg::node_count> gone;
    gone.set(static_cast<std::size_t>(node));
    removed.push_back(static_cast<std::uint16_t>(node));

    for (auto it = at + 1; it != st->queue.end(); ++it)
    {
        const io::dg::node& n = node_at(*it);
        bool needs = false;
        for (int g = 0; g < n.group_count && !needs; ++g)
        {
            bool touches_gone = false, still_met = false;
            for (const std::int16_t m : n.groups[static_cast<std::size_t>(g)])
            {
                if (m < 0)
                    continue;
                if (gone.test(static_cast<std::size_t>(m)))
                    touches_gone = true;
                if (kept.test(static_cast<std::size_t>(m)))
                    still_met = true;
            }
            needs = touches_gone && !still_met;
        }
        if (needs)
        {
            gone.set(*it);
            removed.push_back(*it);
        }
        else
            kept.set(*it);
    }
    return removed;
}

int unqueue_with_dependents(world& w, entity_id nation, int node)
{
    const std::vector<std::uint16_t> removed = queued_dependents(w, nation, node);
    if (removed.empty())
        return 0;
    std::vector<std::uint16_t>& q = w.nation_tech.at(nation).queue;
    q.erase(std::remove_if(q.begin(), q.end(),
                           [&](std::uint16_t n) { return in_list(removed, n); }),
            q.end());
    return static_cast<int>(removed.size());
}

bool clear_research_queue(world& w, entity_id nation)
{
    const auto it = w.nation_tech.find(nation);
    if (it == w.nation_tech.end() || it->second.queue.empty())
        return false;
    it->second.queue.clear();
    return true;
}
