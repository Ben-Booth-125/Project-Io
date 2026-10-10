#include "nation_tech.hpp"

#include "components.hpp"
#include "nation_budget.hpp"
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
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
std::int64_t to_milli(double rp)
{
    if (!std::isfinite(rp) || rp <= 0.0)
        return 0;
    return static_cast<std::int64_t>(std::llround(rp * static_cast<double>(k_milli_per_rp)));
}
} // namespace

std::int64_t node_cost_milli(int node, const research_params& p)
{
    if (!valid_node(node))
        return 0;
    return to_milli(static_cast<double>(node_at(node).cost_units)
                    * static_cast<double>(p.rp_per_cost_unit));
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
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return false;
    return nation_holds(w, cit->second.home_nation, node);
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
    // Every deed is prose until the effect pass (BL-1274) gives it a reader on
    // the condition_set substrate. Until then a quest waits in the queue.
    return !node_at(node).is_quest;
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

float research_multiplier(const world& /*w*/, entity_id /*nation*/)
{
    // BL-1274 fills this from the held nodes' "+N% national research" effects.
    return 1.0f;
}

float nation_scalar(const world& /*w*/, entity_id /*nation*/, modifier_subject /*subject*/,
                    float base)
{
    // BL-1274 fills this from the held nodes' machine effects. Identity until
    // then: the same bits back, no arithmetic performed.
    return base;
}

bool structure_unlocked(const world& /*w*/, entity_id /*corp*/, building_type /*type*/)
{
    // No node of the Digitisation tree carries a machine `unlock_structure`
    // effect yet (BL-1274 authors them): every type is open to every nation
    // (NR-1022). When one does, this becomes "the corp's home nation holds
    // every node whose effect unlocks `type`", read through corp_nation_holds.
    return true;
}

bool recipe_unlocked(const world& /*w*/, const recipe_registry& /*reg*/, entity_id /*corp*/,
                     std::uint16_t /*recipe_id*/)
{
    // As structure_unlocked: no node carries an `unlock_recipe` effect yet, so
    // every recipe is open (NR-1022 — the Toolmaker, Bessemer steel and refined
    // copper gates retired with the per-corp tech set).
    return true;
}

int flow_research_queue(world& w, entity_id nation, const research_params& p)
{
    const auto it = w.nation_tech.find(nation);
    if (it == w.nation_tech.end())
        return 0;
    nation_tech_state& st = it->second;

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

            const std::int64_t cost = node_cost_milli(n, p);
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

void clear_research_at_handover(world& w)
{
    w.nation_tech.clear();
}

void advance_research(world& w, const research_params& p, std::vector<research_accrual>* out)
{
    if (w.nations.empty())
        return;

    // Counted in integers, so the unordered corporation walk cannot reorder a sum.
    std::map<entity_id, int> universities;
    std::map<entity_id, int> institutes;
    for (const auto& [corp, cc] : w.corporations)
    {
        for (const entity_id bid : cc.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            const building_component& b = bit->second;
            // Both types staff at zero (components.hpp): a completed, standing
            // building is a staffed one.
            if (b.ticks_remaining > 0 || b.decommissioned)
                continue;
            if (b.type == building_type::university)
            {
                if (const auto tn = w.tile_to_nation.find(b.tile); tn != w.tile_to_nation.end())
                    ++universities[tn->second];
            }
            else if (b.type == building_type::research_institute)
            {
                if (cc.home_nation != null_entity)
                    ++institutes[cc.home_nation];
            }
        }
    }

    std::vector<entity_id> ids;
    ids.reserve(w.nations.size());
    for (const auto& kv : w.nations)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());

    const std::size_t ai = static_cast<std::size_t>(budget_priority::academic_research);
    for (const entity_id nid : ids)
    {
        nation_component& nc = w.nations.at(nid);
        research_accrual row;
        row.nation = nid;

        if (const auto u = universities.find(nid); u != universities.end())
            row.universities = u->second;
        if (const auto r = institutes.find(nid); r != institutes.end())
            row.institutes = r->second;
        row.multiplier = research_multiplier(w, nid);
        const double mult = std::isfinite(row.multiplier) && row.multiplier > 0.0f
                                ? static_cast<double>(row.multiplier) : 0.0;

        // The RP that cost no treasury: the base and the buildings.
        const std::int64_t free_milli = static_cast<std::int64_t>(std::llround(
            static_cast<double>(to_milli(static_cast<double>(p.base_rp_per_nation)
                                         + static_cast<double>(row.universities) * p.rp_per_university
                                         + static_cast<double>(row.institutes) * p.rp_per_institute))
            * mult));

        // What the queue still owes: every queued node's cost, less the pool
        // and less what the free RP will bring this quarter.
        const auto sit = w.nation_tech.find(nid);
        std::int64_t owed = 0;
        if (sit != w.nation_tech.end())
        {
            for (const std::uint16_t n : sit->second.queue)
                owed += node_cost_milli(n, p);
            owed -= sit->second.pool_milli;
        }
        row.owed_milli = std::max<std::int64_t>(0, owed);
        const std::int64_t line_need = std::max<std::int64_t>(0, owed - free_milli);

        // The line's share: run_national_budget's own formula, recomputed on
        // the run_nation_garrison_upkeep precedent (a line with no corp
        // claimant never reaches that pass's report). It pays min(share, what
        // the queue owes) — the garrison's min(share, bill) — as a direct
        // expenditure, credited to nobody. An empty queue owes nothing and
        // spends nothing.
        if (nc.treasury > 0.0f)
        {
            const auto bit = w.nation_budgets.find(nid);
            if (bit != w.nation_budgets.end())
            {
                const nation_budget& bud = bit->second;
                const float reserve   = std::clamp(bud.reserve_fraction, 0.0f, 1.0f);
                const float spendable = nc.treasury * (1.0f - reserve);
                float weight_total = 0.0f;
                for (std::size_t i = 0; i < priority_count; ++i)
                    if (std::isfinite(bud.weights[i]) && bud.weights[i] > 0.0f)
                        weight_total += bud.weights[i];
                if (spendable > 0.0f && weight_total > 0.0f
                    && std::isfinite(bud.weights[ai]) && bud.weights[ai] > 0.0f)
                    row.line_share = std::min(nc.treasury,
                                              spendable * (bud.weights[ai] / weight_total));
            }
        }
        if (row.line_share > 0.0f && line_need > 0 && mult > 0.0)
        {
            const double bill = line_credits_for(static_cast<std::int64_t>(
                                                     std::ceil(static_cast<double>(line_need) / mult)), p);
            row.line_spend = static_cast<float>(std::min(static_cast<double>(row.line_share), bill));
        }
        if (row.line_spend > 0.0f)
            nc.treasury -= row.line_spend;

        const std::int64_t line_milli = static_cast<std::int64_t>(
            std::llround(static_cast<double>(line_rp_milli(row.line_spend, p)) * mult));
        row.rp_milli = free_milli + line_milli;

        // An entry is created only when there is something to hold, so a world
        // where nothing researches keeps an empty map (and its state_hash).
        if (row.rp_milli > 0 || sit != w.nation_tech.end())
        {
            nation_tech_state& st = w.nation_tech[nid];
            st.pool_milli += row.rp_milli;
            row.completed = flow_research_queue(w, nid, p);
        }

        if (out != nullptr)
            out->push_back(row);
    }
}
