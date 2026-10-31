// War (BL-1459, BL-1428) — see war.hpp and docs/politics/RELATIONS.md § 7.

#include "war.hpp"

#include "battle_system.hpp" // battle_tick, battle_dispatch (the daily hook)
#include "hex_neighbors.hpp"
#include "province.hpp"
#include "stance.hpp" // write_hostility, are_friends, canonical_pair
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <unordered_map>

namespace
{

bool is_nation(const world& w, entity_id n)
{
    return n != null_entity && w.nations.find(n) != w.nations.end();
}

/// Sorted, de-duplicated copy of @p goals; `ok` false on a repeat.
std::vector<uint32_t> sorted_goals(const std::vector<uint32_t>& goals, bool& ok)
{
    std::vector<uint32_t> g = goals;
    std::sort(g.begin(), g.end());
    ok = std::adjacent_find(g.begin(), g.end()) == g.end();
    return g;
}

/// 1..3 distinct provinces, each a LAND province politically owned by @p owner.
bool valid_goal_list(const world& w, const std::vector<uint32_t>& goals, entity_id owner)
{
    if (goals.empty() || goals.size() > k_war_goals_max)
        return false;
    bool distinct = true;
    (void)sorted_goals(goals, distinct);
    if (!distinct)
        return false;
    for (const uint32_t g : goals)
    {
        if (g == 0 || g == no_province)
            return false;
        const province* pr = w.provinces.find(g);
        if (pr == nullptr || province_kind_of(w, *pr) != province_kind::land)
            return false;
        if (province_owner(w, g) != owner)
            return false;
    }
    return true;
}

/// Every nation sharing at least one tile edge with @p a or @p b, ascending, the
/// pair themselves excluded. The raster is written cell by cell (each tile writes
/// only its own cell), so the unordered walk over `w.tiles` cannot reach the result.
std::set<entity_id> nations_bordering(const world& w, entity_id a, entity_id b)
{
    struct key_hash
    {
        std::size_t operator()(const std::tuple<entity_id, int, int>& k) const
        {
            const uint64_t x = (static_cast<uint64_t>(std::get<0>(k)) * 0x9E3779B97F4A7C15ull)
                             ^ (static_cast<uint64_t>(static_cast<uint32_t>(std::get<1>(k))) << 20)
                             ^ static_cast<uint64_t>(static_cast<uint32_t>(std::get<2>(k)));
            return static_cast<std::size_t>(x);
        }
    };
    std::unordered_map<std::tuple<entity_id, int, int>, entity_id, key_hash> grid;
    grid.reserve(w.tiles.size());
    for (const auto& [tid, tc] : w.tiles)
        grid.emplace(std::make_tuple(tc.body, tc.grid_x, tc.grid_y), tid);

    std::set<entity_id> out;
    for (const entity_id party : {a, b})
    {
        const auto nit = w.nations.find(party);
        if (nit == w.nations.end())
            continue;
        for (const entity_id tile : nit->second.tiles)
        {
            const auto tit = w.tiles.find(tile);
            if (tit == w.tiles.end())
                continue;
            const auto bit = w.bodies.find(tit->second.body);
            if (bit == w.bodies.end() || bit->second.grid_width <= 0)
                continue;
            const int gw = bit->second.grid_width;
            const int gh = bit->second.grid_height;
            for (int side = 0; side < 6; ++side)
            {
                const hex_neighbors::coord c =
                    hex_neighbors::neighbour(tit->second.grid_x, tit->second.grid_y, side);
                if (c.gy < 0 || c.gy >= gh)
                    continue;
                int nx = c.gx % gw;
                if (nx < 0)
                    nx += gw;
                const auto git = grid.find(std::make_tuple(tit->second.body, nx, c.gy));
                if (git == grid.end())
                    continue;
                const auto oit = w.tile_to_nation.find(git->second);
                if (oit == w.tile_to_nation.end())
                    continue;
                if (oit->second != a && oit->second != b)
                    out.insert(oit->second);
            }
        }
    }
    return out;
}

float measure(const world& w, war_measure m, entity_id declarer, entity_id defender,
              const std::vector<uint32_t>& goals)
{
    switch (m)
    {
        case war_measure::declarer_trust_toward_defender:
            return sentiment_toward(w.sentiment, declarer, defender).trust;
        case war_measure::defender_embargoes_declarer:
        {
            // An embargo is the defender's standing condition_set evaluating FALSE
            // against the declarer (the `request_quote` reading: empty = no embargo).
            const auto it = w.corp_embargo_conditions.find(defender);
            if (it == w.corp_embargo_conditions.end() || it->second.always())
                return 0.0f;
            return evaluate(it->second, w, declarer) ? 0.0f : 1.0f;
        }
        case war_measure::goals_ceded_by_declarer:
        {
            int n = 0;
            for (const uint32_t g : goals)
                for (const province_cession& c : w.province_cessions)
                    if (c.from == declarer && c.to == defender && c.province == g)
                    {
                        ++n;
                        break;
                    }
            return static_cast<float>(n);
        }
        case war_measure::defender_wars_on_declarer_friends:
        {
            int n = 0;
            for (const auto& [key, war] : w.wars) // ascending
                if (key.first == defender && key.second != declarer
                    && are_friends(w, declarer, key.second))
                    ++n;
            return static_cast<float>(n);
        }
    }
    return 0.0f;
}

bool compare(float v, condition_comparator c, float operand)
{
    switch (c)
    {
        case condition_comparator::at_least:     return v >= operand;
        case condition_comparator::greater_than: return v >  operand;
        case condition_comparator::exactly:      return v == operand;
        case condition_comparator::at_most:      return v <= operand;
        case condition_comparator::less_than:    return v <  operand;
    }
    return false;
}

war_record* find_war_mut(world& w, entity_id a, entity_id b)
{
    if (auto it = w.wars.find({a, b}); it != w.wars.end())
        return &it->second;
    if (auto it = w.wars.find({b, a}); it != w.wars.end())
        return &it->second;
    return nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
// Justification rows
// ---------------------------------------------------------------------------

const std::array<war_justification_row, k_war_justification_count>& war_justification_rows()
{
    static const std::array<war_justification_row, k_war_justification_count> rows = {{
        { "grievance",        war_measure::declarer_trust_toward_defender,
          condition_comparator::at_most,  war_operand::adverse_trust_edge, 0.0f },
        { "embargo",          war_measure::defender_embargoes_declarer,
          condition_comparator::at_least, war_operand::fixed,              1.0f },
        { "lost_ground",      war_measure::goals_ceded_by_declarer,
          condition_comparator::at_least, war_operand::fixed,              1.0f },
        { "friend_attacked",  war_measure::defender_wars_on_declarer_friends,
          condition_comparator::at_least, war_operand::fixed,              1.0f },
    }};
    return rows;
}

// ---------------------------------------------------------------------------
// Reads
// ---------------------------------------------------------------------------

entity_id province_owner(const world& w, uint32_t province_id)
{
    const province* pr = w.provinces.find(province_id);
    if (pr == nullptr)
        return null_entity;
    std::map<entity_id, int> tally; // ordered: the strict '>' scan keeps the lowest id on a tie
    for (const entity_id tile : pr->tiles)
        if (const auto it = w.tile_to_nation.find(tile); it != w.tile_to_nation.end())
            ++tally[it->second];
    entity_id best = null_entity;
    int       best_n = 0;
    for (const auto& [n, c] : tally)
        if (c > best_n)
        {
            best   = n;
            best_n = c;
        }
    return best;
}

uint32_t attack_goal_province(const world& w, entity_id defender, entity_id tile)
{
    if (!is_nation(w, defender))
        return 0;
    const uint32_t p = w.provinces.province_of(tile);
    if (p != 0 && province_owner(w, p) == defender
        && province_kind_of(w, p) == province_kind::land)
        return p;
    // The attacked stack stands on ground that is not the defender's: the goal
    // falls back to the defender's capital province (a design call — see the
    // BL-1459 report).
    const entity_id cap = w.nations.at(defender).capital_tile;
    const uint32_t cp = cap == null_entity ? 0u : w.provinces.province_of(cap);
    if (cp != 0 && province_owner(w, cp) == defender)
        return cp;
    return 0;
}

const war_record* war_between(const world& w, entity_id a, entity_id b)
{
    if (auto it = w.wars.find({a, b}); it != w.wars.end())
        return &it->second;
    if (auto it = w.wars.find({b, a}); it != w.wars.end())
        return &it->second;
    return nullptr;
}

bool at_war(const world& w, entity_id a, entity_id b)
{
    return war_between(w, a, b) != nullptr;
}

bool truce_between(const world& w, entity_id a, entity_id b)
{
    const auto it = w.truces.find(canonical_pair(a, b));
    return it != w.truces.end() && w.current_econ_tick < it->second;
}

std::array<bool, k_war_justification_count>
war_justifications(const world& w, const sentiment_params& p, entity_id declarer,
                   entity_id defender, const std::vector<uint32_t>& goals)
{
    std::array<bool, k_war_justification_count> out{};
    const auto& rows = war_justification_rows();
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        const war_justification_row& r = rows[i];
        const float operand = r.operand_source == war_operand::adverse_trust_edge
                                ? -(p.band_edge_far * p.limit)
                                : r.operand;
        out[i] = compare(measure(w, r.measure, declarer, defender, goals), r.comparator, operand);
    }
    return out;
}

namespace
{
/// Every refusal `declare_war` makes, read-only, in its order.
war_result declare_check(const world& w, entity_id declarer, entity_id defender,
                         const std::vector<uint32_t>& goals)
{
    if (declarer == defender || !is_nation(w, declarer) || !is_nation(w, defender))
        return war_result::rejected_invalid;
    if (!valid_goal_list(w, goals, defender))
        return war_result::rejected_invalid;
    if (at_war(w, declarer, defender))
        return war_result::rejected_state;
    if (truce_between(w, declarer, defender))
        return war_result::rejected_state;
    return war_result::applied;
}

war_price_row priced(const sentiment_params& p, entity_id observer, sentiment_factor_kind kind,
                     float magnitude, bool borders)
{
    war_price_row r;
    r.observer  = observer;
    r.kind      = kind;
    r.magnitude = magnitude;
    const sentiment_factor& f = p.factors[static_cast<std::size_t>(kind)];
    r.access    = f.access * magnitude;
    r.trust     = f.trust * magnitude;
    r.borders   = borders;
    return r;
}
} // namespace

war_price declaration_price(const world& w, const sentiment_params& p, entity_id declarer,
                            entity_id defender, const std::vector<uint32_t>& goals)
{
    war_price out;
    if (declare_check(w, declarer, defender, goals) != war_result::applied)
        return out;
    out.valid          = true;
    out.justifications = war_justifications(w, p, declarer, defender, goals);
    out.justified      = std::any_of(out.justifications.begin(), out.justifications.end(),
                                     [](bool b) { return b; });

    // hostility_declared: the defender toward the declarer, once.
    out.rows.push_back(priced(p, defender, sentiment_factor_kind::hostility_declared, 1.0f, false));

    // war_declared: every other nation toward the declarer.
    const std::set<entity_id> border = nations_bordering(w, declarer, defender);
    const float goal_factor =
        1.0f + k_war_per_extra_goal * static_cast<float>(goals.size() - 1);
    std::vector<entity_id> others;
    others.reserve(w.nations.size());
    for (const auto& [nid, nc] : w.nations)
        if (nid != declarer && nid != defender)
            others.push_back(nid);
    std::sort(others.begin(), others.end());
    for (const entity_id o : others)
    {
        const bool b = border.count(o) != 0;
        float m = goal_factor * (b ? k_war_border_multiplier : 1.0f);
        if (out.justified)
            m *= k_war_justified_multiplier;
        out.rows.push_back(priced(p, o, sentiment_factor_kind::war_declared, m, b));
    }
    for (const war_price_row& r : out.rows)
    {
        out.sum_access += r.access;
        out.sum_trust  += r.trust;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Verbs
// ---------------------------------------------------------------------------

war_result declare_war(world& w, const sentiment_params& p, entity_id declarer,
                       entity_id defender, const std::vector<uint32_t>& goals)
{
    const war_result chk = declare_check(w, declarer, defender, goals);
    if (chk != war_result::applied)
        return chk;
    // The price is computed BEFORE any write, so it reads the world the preview
    // read: `declare_war` charges exactly `declaration_price`.
    const war_price price = declaration_price(w, p, declarer, defender, goals);

    bool distinct = true;
    war_record rec;
    rec.declarer       = declarer;
    rec.defender       = defender;
    rec.declarer_goals = sorted_goals(goals, distinct);
    rec.opened_tick    = w.current_econ_tick;
    rec.opened_day     = w.construction_quarter_day;

    // Validation is complete; everything below is unconditional.
    write_hostility(w, declarer, defender);
    w.wars[{declarer, defender}] = std::move(rec);

    std::vector<sentiment_event> events;
    events.reserve(price.rows.size());
    for (const war_price_row& r : price.rows)
        events.push_back(sentiment_event{ r.observer, declarer, r.kind, r.magnitude });
    apply_sentiment_events(w.sentiment, p, events);
    return war_result::applied;
}

war_result answer_war(world& w, entity_id defender, entity_id declarer,
                      const std::vector<uint32_t>& goals)
{
    if (declarer == defender || !is_nation(w, declarer) || !is_nation(w, defender))
        return war_result::rejected_invalid;
    if (!valid_goal_list(w, goals, declarer))
        return war_result::rejected_invalid;
    const auto it = w.wars.find({declarer, defender});
    if (it == w.wars.end() || it->second.answered)
        return war_result::rejected_state;

    bool distinct = true;
    write_hostility(w, defender, declarer);
    it->second.defender_goals = sorted_goals(goals, distinct);
    it->second.answered       = true;
    return war_result::applied;
}

// ---------------------------------------------------------------------------
// Peace — W3 (BL-1428) fills these; until then the seam refuses every peace verb.
// ---------------------------------------------------------------------------

war_result offer_peace(world&, entity_id, entity_id, const peace_terms&) { return war_result::rejected_state; }
war_result accept_peace(world&, const sentiment_params&, entity_id, entity_id) { return war_result::rejected_state; }
war_result refuse_peace(world&, entity_id, entity_id) { return war_result::rejected_state; }
war_result impose_peace(world&, const sentiment_params&, entity_id, entity_id, const peace_terms&)
{
    return war_result::rejected_state;
}
