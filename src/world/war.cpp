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
// War score, exhaustion and morale (BL-1428)
// ---------------------------------------------------------------------------

int war_score(const world& w, const war_record& war)
{
    // Occupation: tile-weighted goals held, the declarer's less the defender's, over
    // every goal named. A goal an in-play merge has removed weighs nothing.
    int64_t total = 0, dec_held = 0, def_held = 0;
    for (const uint32_t g : war.declarer_goals)
        if (const province* pr = w.provinces.find(g))
        {
            const int64_t n = static_cast<int64_t>(pr->tiles.size());
            total += n;
            if (province_holder_for(w, g) == war.declarer)
                dec_held += n;
        }
    for (const uint32_t g : war.defender_goals)
        if (const province* pr = w.provinces.find(g))
        {
            const int64_t n = static_cast<int64_t>(pr->tiles.size());
            total += n;
            if (province_holder_for(w, g) == war.defender)
                def_held += n;
        }
    const double occupation =
        total > 0 ? static_cast<double>(dec_held - def_held) / static_cast<double>(total) : 0.0;
    // Battles: the strength each side destroyed — the other side's men lost.
    const int64_t lost = war.declarer_men_lost + war.defender_men_lost;
    const double battles =
        lost > 0 ? static_cast<double>(war.defender_men_lost - war.declarer_men_lost)
                       / static_cast<double>(lost)
                 : 0.0;
    const long long s = std::llround(50.0 * occupation + 50.0 * battles);
    return static_cast<int>(std::clamp<long long>(s, -100, 100));
}

float nation_war_exhaustion(const world& w, entity_id nation)
{
    float e = 0.0f;
    if (const auto it = w.war_exhaustion_residual.find(nation); it != w.war_exhaustion_residual.end())
        e = it->second;
    for (const auto& [key, war] : w.wars)
    {
        if (war.declarer == nation) e = std::max(e, war.declarer_exhaustion);
        if (war.defender == nation) e = std::max(e, war.defender_exhaustion);
    }
    return std::clamp(e, 0.0f, 100.0f);
}

int war_morale_ceiling(const world& w, entity_id nation)
{
    const float e = nation_war_exhaustion(w, nation);
    const int drop = static_cast<int>(e * static_cast<float>(k_war_morale_per_exhaustion) + 0.5f);
    return std::clamp(1000 - drop, 0, 1000);
}

namespace
{
/// Every nation whose exhaustion lowers its morale ceiling, with that ceiling.
std::map<entity_id, int> morale_ceilings(const world& w)
{
    std::map<entity_id, int> out;
    if (w.wars.empty() && w.war_exhaustion_residual.empty())
        return out;
    std::set<entity_id> parties;
    for (const auto& [key, war] : w.wars) { parties.insert(war.declarer); parties.insert(war.defender); }
    for (const auto& [n, v] : w.war_exhaustion_residual) parties.insert(n);
    for (const entity_id n : parties)
        if (const int c = war_morale_ceiling(w, n); c < 1000)
            out.emplace(n, c);
    return out;
}

float add_exhaustion(float e, float d) { return std::clamp(e + d, 0.0f, 100.0f); }
} // namespace

void apply_war_morale_ceiling(world& w)
{
    const std::map<entity_id, int> ceil = morale_ceilings(w);
    if (ceil.empty())
        return;
    // Each unit's write reads only itself and its nation's ceiling, so the walk
    // order over the unordered unit store cannot reach the result.
    for (auto& [uid, u] : w.units)
    {
        const auto it = ceil.find(u.owner);
        if (it == ceil.end())
            continue;
        u.morale_permille    = std::min(u.morale_permille, it->second);
        u.readiness_permille = std::min(u.readiness_permille, u.morale_permille);
    }
}

void note_war_battles(world& w, const battle_tick& battles)
{
    if (w.wars.empty() || battles.dispatches.empty())
        return;
    // Men standing per nation, read once (after the day's rounds, before the
    // disband): the denominator of the loss share.
    std::map<entity_id, int64_t> standing;
    bool counted = false;
    const auto men_of = [&](entity_id n) {
        if (!counted)
        {
            for (const auto& [uid, u] : w.units) // integer sums: order-free
                if (u.count > 0) standing[u.owner] += u.count;
            counted = true;
        }
        const auto it = standing.find(n);
        return it == standing.end() ? int64_t{0} : it->second;
    };
    const auto exhaust = [&](float& e, entity_id n, int lost) {
        if (lost <= 0) return;
        const double share = static_cast<double>(lost) / static_cast<double>(men_of(n) + lost);
        e = add_exhaustion(e, static_cast<float>(k_war_exhaustion_loss_full * share));
    };
    bool any = false;
    for (const battle_dispatch& d : battles.dispatches) // battle order: sorted
    {
        war_record* war = find_war_mut(w, d.attacker, d.defender);
        if (war == nullptr)
            continue;
        any = true;
        const bool att_is_dec = d.attacker == war->declarer;
        const int dec_lost = att_is_dec ? d.attacker_men_lost : d.defender_men_lost;
        const int def_lost = att_is_dec ? d.defender_men_lost : d.attacker_men_lost;
        war->declarer_men_lost += std::max(0, dec_lost);
        war->defender_men_lost += std::max(0, def_lost);
        exhaust(war->declarer_exhaustion, war->declarer, dec_lost);
        exhaust(war->defender_exhaustion, war->defender, def_lost);
    }
    if (any)
        apply_war_morale_ceiling(w);
}

void accumulate_war_field_days(world& w, int days)
{
    if (w.wars.empty() || days <= 0)
        return;
    // Per war, each party's units in the field: standing on the other party's
    // ground, or fighting it in a live battle — as a share of its standing units.
    std::map<std::pair<entity_id, entity_id>, std::set<entity_id>> fighting; // (owner, enemy) -> units
    for (const active_battle& b : w.battles)
    {
        if (b.state.end != campaign_battle_end::in_progress)
            continue;
        for (const entity_id u : b.attacker_units) fighting[{ b.attacker, b.defender }].insert(u);
        for (const entity_id u : b.defender_units) fighting[{ b.defender, b.attacker }].insert(u);
    }
    // (owner, enemy) -> units in the field; owner -> units standing. Integer
    // counts, so the unordered walk cannot reach them.
    std::map<std::pair<entity_id, entity_id>, int> in_field;
    std::map<entity_id, int> units_of;
    std::map<entity_id, std::vector<entity_id>> enemies;
    for (const auto& [key, war] : w.wars)
    {
        enemies[war.declarer].push_back(war.defender);
        enemies[war.defender].push_back(war.declarer);
    }
    for (const auto& [uid, u] : w.units)
    {
        if (u.count <= 0)
            continue;
        ++units_of[u.owner];
        const auto eit = enemies.find(u.owner);
        if (eit == enemies.end())
            continue;
        const auto tn = w.tile_to_nation.find(u.position);
        for (const entity_id enemy : eit->second)
        {
            const bool abroad = tn != w.tile_to_nation.end() && tn->second == enemy;
            const auto fit = fighting.find({ u.owner, enemy });
            const bool fights = fit != fighting.end() && fit->second.count(uid) != 0;
            if (abroad || fights)
                ++in_field[{ u.owner, enemy }];
        }
    }
    const auto share = [&](entity_id n, entity_id enemy) {
        const auto tot = units_of.find(n);
        const auto f = in_field.find({ n, enemy });
        if (tot == units_of.end() || tot->second <= 0 || f == in_field.end())
            return 0.0f;
        return static_cast<float>(f->second) / static_cast<float>(tot->second);
    };
    const float d = static_cast<float>(days);
    for (auto& [key, war] : w.wars) // ascending
    {
        war.declarer_field_days += d * share(war.declarer, war.defender);
        war.defender_field_days += d * share(war.defender, war.declarer);
    }
}

void run_war_quarter(world& w)
{
    // Expired truces go (a truce stands while current_econ_tick < expiry, so the
    // read is the same whenever this runs).
    for (auto it = w.truces.begin(); it != w.truces.end();)
        it = (w.current_econ_tick >= it->second) ? w.truces.erase(it) : std::next(it);

    if (!w.wars.empty())
    {
        // Provinces each party owns that the other holds: walk the holders once.
        std::map<std::pair<entity_id, entity_id>, int> held; // (owner, holder) -> provinces
        std::set<entity_id> at_war_nations;
        for (const auto& [key, war] : w.wars) { at_war_nations.insert(war.declarer); at_war_nations.insert(war.defender); }
        for (std::size_t i = 0; i < w.provinces.provinces.size() && i < w.province_holder.size(); ++i)
        {
            const entity_id holder = w.province_holder[i];
            if (holder == null_entity || at_war_nations.count(holder) == 0)
                continue;
            const entity_id owner = province_owner(w, w.provinces.provinces[i].id);
            if (owner != null_entity && owner != holder)
                ++held[{ owner, holder }];
        }
        const auto held_by = [&](entity_id owner, entity_id holder) {
            const auto it = held.find({ owner, holder });
            return it == held.end() ? 0 : it->second;
        };

        // The field days the quarter's days accumulated (`accumulate_war_field_days`)
        // are charged here and reset.
        for (auto& [key, war] : w.wars) // ascending (declarer, defender)
        {
            war.declarer_exhaustion = add_exhaustion(
                war.declarer_exhaustion,
                k_war_exhaustion_per_field_day * war.declarer_field_days
                    + k_war_exhaustion_per_held_province * static_cast<float>(held_by(war.declarer, war.defender)));
            war.defender_exhaustion = add_exhaustion(
                war.defender_exhaustion,
                k_war_exhaustion_per_field_day * war.defender_field_days
                    + k_war_exhaustion_per_held_province * static_cast<float>(held_by(war.defender, war.declarer)));
            war.declarer_field_days = 0.0f;
            war.defender_field_days = 0.0f;
        }
    }

    // A nation in no open war sheds its residual.
    for (auto it = w.war_exhaustion_residual.begin(); it != w.war_exhaustion_residual.end();)
    {
        bool fighting_now = false;
        for (const auto& [key, war] : w.wars)
            if (war.declarer == it->first || war.defender == it->first) { fighting_now = true; break; }
        if (!fighting_now)
            it->second *= (1.0f - k_war_exhaustion_decay);
        it = (it->second < k_war_exhaustion_epsilon) ? w.war_exhaustion_residual.erase(it) : std::next(it);
    }

    apply_war_morale_ceiling(w);
}

// ---------------------------------------------------------------------------
// Peace (BL-1428)
// ---------------------------------------------------------------------------

namespace
{
std::pair<entity_id, entity_id> war_key(const war_record& war) { return { war.declarer, war.defender }; }

/// Land provinces @p nation owns (politically), counted over the provinces its
/// tiles touch.
std::size_t provinces_owned(const world& w, entity_id nation)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return 0;
    std::set<uint32_t> touched;
    for (const entity_id t : nit->second.tiles)
        if (const uint32_t p = w.provinces.province_of(t); p != 0)
            touched.insert(p);
    std::size_t n = 0;
    for (const uint32_t p : touched)
        if (province_kind_of(w, p) == province_kind::land && province_owner(w, p) == nation)
            ++n;
    return n;
}

void set_holder(world& w, uint32_t pid, entity_id holder)
{
    const auto& v = w.provinces.provinces;
    const auto it = std::lower_bound(v.begin(), v.end(), pid,
                                     [](const province& p, uint32_t x) { return p.id < x; });
    if (it == v.end() || it->id != pid)
        return;
    const std::size_t i = static_cast<std::size_t>(it - v.begin());
    if (i < w.province_holder.size())
        w.province_holder[i] = holder;
}

/// Closes the war under @p key on @p terms (already validated). One mutation.
void conclude_peace(world& w, const sentiment_params& p, std::pair<entity_id, entity_id> key,
                    const peace_terms& terms, bool imposed)
{
    const war_record war = w.wars.at(key);
    war_history_entry h;
    h.declarer    = war.declarer;
    h.defender    = war.defender;
    h.opened_tick = war.opened_tick;
    h.closed_tick = w.current_econ_tick;
    h.final_score = war_score(w, war);

    std::vector<sentiment_event> events;
    events.push_back(sentiment_event{ war.declarer, war.defender, sentiment_factor_kind::peace_concluded, 1.0f });
    events.push_back(sentiment_event{ war.defender, war.declarer, sentiment_factor_kind::peace_concluded, 1.0f });

    if (terms.kind == peace_kind::cession)
    {
        const entity_id loser = terms.gainer == war.declarer ? war.defender : war.declarer;
        cede_provinces(w, loser, terms.gainer, terms.provinces);
        h.end    = imposed ? war_end::imposed_cession : war_end::cession;
        h.gainer = terms.gainer;
        h.ceded  = terms.provinces;
        events.push_back(sentiment_event{ loser, terms.gainer, sentiment_factor_kind::province_ceded,
                                          static_cast<float>(terms.provinces.size()) });
    }
    else
    {
        // White peace: every occupied goal's holder goes back to its owner.
        // TODO(BL-1460, cession moves territory): the occupying units withdraw by the
        // retreat rule; for now they stay where they stand.
        for (const std::vector<uint32_t>* goals : { &war.declarer_goals, &war.defender_goals })
            for (const uint32_t g : *goals)
            {
                const entity_id owner = province_owner(w, g);
                if (owner != null_entity && province_holder_for(w, g) != owner)
                    set_holder(w, g, owner);
            }
        h.end = imposed ? war_end::imposed_white : war_end::white_peace;
    }

    // Every battle still being fought between the pair ENDS with the peace, closed
    // as drawn: erased with no dispatch, no retreat and no holder write, so nothing
    // a peace settled (a white peace's returned holders, a cession) is undone by a
    // fight that outlived it. Its units are out of contact from now on.
    w.battles.erase(
        std::remove_if(w.battles.begin(), w.battles.end(),
                       [&](const active_battle& b) {
                           return (b.attacker == war.declarer && b.defender == war.defender)
                               || (b.attacker == war.defender && b.defender == war.declarer);
                       }),
        w.battles.end());

    // Both hostility rows clear; the record closes; the offer goes; a truce opens.
    w.corp_hostile_pairs.erase({ war.declarer, war.defender });
    w.corp_hostile_pairs.erase({ war.defender, war.declarer });
    for (const auto& [n, e] : { std::make_pair(war.declarer, war.declarer_exhaustion),
                                std::make_pair(war.defender, war.defender_exhaustion) })
        if (e >= k_war_exhaustion_epsilon)
        {
            float& r = w.war_exhaustion_residual[n];
            r = std::max(r, e);
        }
    w.war_history.push_back(std::move(h));
    w.peace_offers.erase(key);
    w.wars.erase(key);
    w.truces[canonical_pair(war.declarer, war.defender)] = w.current_econ_tick + k_war_truce_ticks;
    apply_sentiment_events(w.sentiment, p, events);
}

war_result pair_check(const world& w, entity_id a, entity_id b)
{
    if (a == b || !is_nation(w, a) || !is_nation(w, b))
        return war_result::rejected_invalid;
    if (!at_war(w, a, b))
        return war_result::rejected_state;
    return war_result::applied;
}
} // namespace

war_result check_peace_terms(const world& w, const war_record& war, const peace_terms& terms)
{
    if (terms.kind == peace_kind::white)
        return (terms.gainer == null_entity && terms.provinces.empty()) ? war_result::applied
                                                                        : war_result::rejected_invalid;
    if (terms.kind != peace_kind::cession)
        return war_result::rejected_invalid;
    if (terms.gainer != war.declarer && terms.gainer != war.defender)
        return war_result::rejected_invalid;
    if (terms.provinces.empty() || terms.provinces.size() > k_war_goals_max)
        return war_result::rejected_invalid;
    bool distinct = true;
    if (sorted_goals(terms.provinces, distinct) != terms.provinces || !distinct)
        return war_result::rejected_invalid; // ascending, distinct
    const entity_id loser = terms.gainer == war.declarer ? war.defender : war.declarer;
    const std::vector<uint32_t>& goals =
        terms.gainer == war.declarer ? war.declarer_goals : war.defender_goals;
    for (const uint32_t pid : terms.provinces)
    {
        if (std::find(goals.begin(), goals.end(), pid) == goals.end())
            return war_result::rejected_state; // only goals
        if (province_holder_for(w, pid) != terms.gainer)
            return war_result::rejected_state; // only held ones
        if (province_owner(w, pid) != loser)
            return war_result::rejected_state; // still the loser's to cede
    }
    // Never the loser's last province (Ben, 2026-10-30, NR-1102).
    if (provinces_owned(w, loser) <= terms.provinces.size())
        return war_result::rejected_state;
    return war_result::applied;
}

war_result offer_peace(world& w, entity_id offerer, entity_id other, const peace_terms& terms)
{
    if (const war_result r = pair_check(w, offerer, other); r != war_result::applied)
        return r;
    const war_record& war = *war_between(w, offerer, other);
    const auto key = war_key(war);
    if (w.peace_offers.count(key) != 0)
        return war_result::rejected_state; // one pending offer per war
    if (const war_result r = check_peace_terms(w, war, terms); r != war_result::applied)
        return r;
    peace_offer o;
    o.offerer      = offerer;
    o.terms        = terms;
    o.offered_tick = w.current_econ_tick;
    w.peace_offers.emplace(key, std::move(o));
    return war_result::applied;
}

war_result accept_peace(world& w, const sentiment_params& p, entity_id receiver, entity_id offerer)
{
    if (const war_result r = pair_check(w, receiver, offerer); r != war_result::applied)
        return r;
    const auto key = war_key(*war_between(w, receiver, offerer));
    const auto oit = w.peace_offers.find(key);
    if (oit == w.peace_offers.end() || oit->second.offerer != offerer)
        return war_result::rejected_state;
    const peace_terms terms = oit->second.terms;
    if (check_peace_terms(w, w.wars.at(key), terms) != war_result::applied)
        return war_result::rejected_state; // the terms no longer hold; the offer stays
    conclude_peace(w, p, key, terms, false);
    return war_result::applied;
}

war_result refuse_peace(world& w, entity_id receiver, entity_id offerer)
{
    if (const war_result r = pair_check(w, receiver, offerer); r != war_result::applied)
        return r;
    const auto key = war_key(*war_between(w, receiver, offerer));
    const auto oit = w.peace_offers.find(key);
    if (oit == w.peace_offers.end() || oit->second.offerer != offerer)
        return war_result::rejected_state;
    w.peace_offers.erase(oit);
    return war_result::applied;
}

war_result impose_peace(world& w, const sentiment_params& p, entity_id imposer, entity_id other,
                        const peace_terms& terms)
{
    if (const war_result r = pair_check(w, imposer, other); r != war_result::applied)
        return r;
    const war_record& war = *war_between(w, imposer, other);
    const int score = war_score(w, war);
    const bool won = imposer == war.declarer ? score >= 100 : score <= -100;
    if (const war_result r = check_peace_terms(w, war, terms); r != war_result::applied)
        return r;
    if (!won)
        return war_result::rejected_state;
    conclude_peace(w, p, war_key(war), terms, true);
    return war_result::applied;
}

void cede_provinces(world& w, entity_id from, entity_id to, const std::vector<uint32_t>& provinces)
{
    // THE STUB (BL-1460, cession moves territory, fills it in wave 2). Today a
    // cession moves only the military holder — already `to` for a legal cession —
    // and records the cession for the LOST GROUND justification. `tile_to_nation`
    // is deliberately NOT moved here: the ground changing nation (buildings,
    // centres, law, roads, fronts, the capital) is BL-1460's.
    for (const uint32_t pid : provinces)
    {
        set_holder(w, pid, to);
        w.province_cessions.push_back(province_cession{ from, to, pid, w.current_econ_tick });
    }
}
