// War (BL-1459, BL-1428) — see war.hpp and docs/politics/RELATIONS.md § 7.

#include "war.hpp"

#include "battle_system.hpp" // battle_tick, battle_dispatch (the daily hook)
#include "hex_neighbors.hpp"
#include "modifier_set.hpp"      // modifier_subject::alarm_toward, effect_target
#include "nation_tech.hpp"       // nation_scalar: the alarm scale the fold stamps
#include "embargo.hpp" // BL-1418: the embargo justification reads embargoes()
#include "sentiment_emitters.hpp" // queue_conduct, sentiment_conduct_live (BL-1458)
#include "province.hpp"
#include "stance.hpp" // write_hostility, are_friends, canonical_pair
#include "logistics.hpp"      // invalidate_logistics_caches (BL-1460)
#include "state_industry.hpp" // capital_free_source (BL-1460)
#include "fronts.hpp"         // reanchor_fronts (BL-1460)
#include "economy_system.hpp" // military_quarter_day_base (BL-1460 fix round)
#include "air_combat.hpp"     // air_tile_distance: a wing's rebase (BL-1460 fix round)
#include "offworld_base.hpp"  // offworld_base_capital (BL-1602: a capital on each body)

#include <limits>
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <unordered_map>
#include <type_traits>

namespace
{

/// A live nation: known and not ended (BL-1562, NATIONS.md § Elimination). Every verb
/// below refuses an ended nation as either party.
bool is_nation(const world& w, entity_id n)
{
    if (n == null_entity)
        return false;
    const auto it = w.nations.find(n);
    return it != w.nations.end() && !it->second.ended;
}

std::string nation_label(const world& w, entity_id n)
{
    const auto it = w.nations.find(n);
    return it != w.nations.end() && !it->second.name.empty() ? it->second.name
                                                             : "Nation " + std::to_string(n);
}

/// BL-1480 (Ben, 2026-11-03, NR-1120; RELATIONS.md § 7, "The public record"): a
/// declaration or a peace writes the world history log, ONE ENTRY PER PARTY tagged by
/// that party's nation in `corp` - the trade-route precedent (one event, two tags), so
/// a nation-scoped read of the log, and the player's dispatch, finds every war it is in.
void log_war_event(world& w, entity_id a, entity_id b, const std::string& event,
                   const std::string& consequence)
{
    for (const entity_id party : { a, b })
    {
        world_history_entry e;
        e.timestamp   = w.current_day_tick;
        e.topic       = history_topic::war;
        e.corp        = party;
        e.event       = event;
        e.consequence = consequence;
        w.history_log.push_back(std::move(e));
    }
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
            // BL-1418 (RELATIONS.md § 3): the defender's embargo of the declarer.
            return embargoes(w, defender, declarer) ? 1.0f : 0.0f;
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

/// The day stamp a peace's withdrawal writes on the units it moves (`tile_since_day`,
/// BL-1453): the stamp the day being walked carries — the quarter base of the economy
/// step that will CLOSE this quarter (`military_quarter_day_base(current_econ_tick +
/// 1)`: a peace is concluded between steps, where `current_econ_tick` is the step last
/// run) plus the days of the quarter already walked. The same scheme the day loop
/// stamps battles and advances with, so it never depends on the UI's day clock.
int peace_day_stamp(const world& w)
{
    return military_quarter_day_base(w.current_econ_tick + 1) + w.construction_quarter_day;
}

/// BL-1706: the war entry in which @p a and @p b stand across (leaders' key first).
const std::pair<const war_key_t, war_record>* war_entry_between(const world& w, entity_id a, entity_id b)
{
    if (a == b || a == null_entity || b == null_entity)
        return nullptr;
    if (auto it = w.wars.find({a, b}); it != w.wars.end())
        return &*it;
    if (auto it = w.wars.find({b, a}); it != w.wars.end())
        return &*it;
    for (const auto& kv : w.wars) // ascending
    {
        if (kv.second.joiners.empty())
            continue;
        const int sa = war_side(kv.second, a);
        if (sa < 0)
            continue;
        const int sb = war_side(kv.second, b);
        if (sb >= 0 && sb != sa)
            return &kv;
    }
    return nullptr;
}

war_record* find_war_mut(world& w, entity_id a, entity_id b)
{
    const auto* e = war_entry_between(w, a, b);
    return e == nullptr ? nullptr : &w.wars.at(e->first);
}

bool contains(const std::vector<entity_id>& v, entity_id n)
{
    return std::find(v.begin(), v.end(), n) != v.end();
}

/// The goal list @p member named, mutable (its leader list or its joiner entry).
std::vector<uint32_t>* member_goals_mut(war_record& war, entity_id member)
{
    if (member == war.declarer) return &war.declarer_goals;
    if (member == war.defender) return &war.defender_goals;
    for (war_member& m : war.joiners)
        if (m.nation == member)
            return &m.goals;
    return nullptr;
}

/// Every pair standing across in @p war: (side-0 member, side-1 member), side order.
std::vector<std::pair<entity_id, entity_id>> cross_pairs(const war_record& war)
{
    std::vector<std::pair<entity_id, entity_id>> out;
    const std::vector<entity_id> s0 = war_side_members(war, 0), s1 = war_side_members(war, 1);
    for (const entity_id a : s0)
        for (const entity_id b : s1)
            out.emplace_back(a, b);
    return out;
}

/// BL-1480 public record, one entry per party in @p parties (in that order).
void log_war_event_all(world& w, const std::vector<entity_id>& parties, const std::string& event,
                       const std::string& consequence)
{
    for (const entity_id party : parties)
    {
        world_history_entry e;
        e.timestamp   = w.current_day_tick;
        e.topic       = history_topic::war;
        e.corp        = party;
        e.event       = event;
        e.consequence = consequence;
        w.history_log.push_back(std::move(e));
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Sides (BL-1706, wars draw in allies; RELATIONS.md § Joining a war)
// ---------------------------------------------------------------------------

int war_side(const war_record& war, entity_id nation)
{
    if (nation == null_entity)
        return -1;
    if (nation == war.declarer) return 0;
    if (nation == war.defender) return 1;
    for (const war_member& m : war.joiners)
        if (m.nation == nation)
            return m.side;
    return -1;
}

entity_id war_leader(const war_record& war, int side)
{
    return side == 0 ? war.declarer : war.defender;
}

std::vector<entity_id> war_side_members(const war_record& war, int side)
{
    std::vector<entity_id> out;
    out.push_back(war_leader(war, side));
    for (const war_member& m : war.joiners)
        if (m.side == side)
            out.push_back(m.nation);
    return out;
}

std::vector<uint32_t> war_side_goals(const war_record& war, int side)
{
    std::vector<uint32_t> out = side == 0 ? war.declarer_goals : war.defender_goals;
    for (const war_member& m : war.joiners)
        if (m.side == side)
            out.insert(out.end(), m.goals.begin(), m.goals.end());
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

const std::vector<uint32_t>* war_member_goals(const war_record& war, entity_id member)
{
    if (member == war.declarer) return &war.declarer_goals;
    if (member == war.defender) return &war.defender_goals;
    for (const war_member& m : war.joiners)
        if (m.nation == member)
            return &m.goals;
    return nullptr;
}

entity_id war_goal_namer(const war_record& war, int side, uint32_t province)
{
    for (const entity_id m : war_side_members(war, side))
    {
        const std::vector<uint32_t>* g = war_member_goals(war, m);
        if (g != nullptr && std::binary_search(g->begin(), g->end(), province))
            return m;
    }
    return null_entity;
}

int64_t war_side_men_lost(const war_record& war, int side)
{
    int64_t n = (side == 0 ? war.declarer_men_lost : war.defender_men_lost) + war.departed_men_lost[side == 0 ? 0 : 1];
    for (const war_member& m : war.joiners)
        if (m.side == side)
            n += m.men_lost;
    return n;
}

const war_record* war_of_member(const world& w, entity_id nation, entity_id across, war_key_t* key)
{
    const auto* e = war_entry_between(w, nation, across);
    if (e == nullptr)
        return nullptr;
    if (key != nullptr)
        *key = e->first;
    return &e->second;
}

// ---------------------------------------------------------------------------
// Justification rows
// ---------------------------------------------------------------------------

const std::array<war_justification_row, k_war_justification_count>& war_justification_rows()
{
    static const std::array<war_justification_row, k_war_justification_count> rows = {{
        // Grievance holds in the HOSTILE-LEANING band, Trust <= -15 (Ben, 2026-11-03,
        // NR-1104, NR-1129): the adverse band (-50), then -25, were rules conduct never met.
        { "grievance",        war_measure::declarer_trust_toward_defender,
          condition_comparator::at_most,  war_operand::fixed,              k_war_grievance_trust },
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
    // BL-1459 report). BL-1602 (MILITARY.md § Space, "A war spans every body"): on
    // another body than its capital's, that is the defender's capital ON THAT BODY,
    // its base (`offworld_base_capital`), where it has one; else the capital as ever.
    entity_id cap = w.nations.at(defender).capital_tile;
    if (const auto tt = w.tiles.find(tile); tt != w.tiles.end())
        if (const auto ct = w.tiles.find(cap); ct == w.tiles.end() || ct->second.body != tt->second.body)
            if (const entity_id base = offworld_base_capital(w, defender, tt->second.body); base != null_entity)
                cap = base;
    const uint32_t cp = cap == null_entity ? 0u : w.provinces.province_of(cap);
    if (cp != 0 && province_owner(w, cp) == defender)
        return cp;
    return 0;
}

const war_record* war_between(const world& w, entity_id a, entity_id b)
{
    const auto* e = war_entry_between(w, a, b);
    return e == nullptr ? nullptr : &e->second;
}

bool at_war(const world& w, entity_id a, entity_id b)
{
    return war_entry_between(w, a, b) != nullptr;
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
    (void)p; // every operand is authored on its row (NR-1104 retired the band read)
    std::array<bool, k_war_justification_count> out{};
    const auto& rows = war_justification_rows();
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        const war_justification_row& r = rows[i];
        out[i] = compare(measure(w, r.measure, declarer, defender, goals), r.comparator, r.operand);
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

/// The alarm scale `fold_pending_sentiment` stamps on an event about @p subject:
/// its "Alarm toward you" scalar (the same read, the same fallback).
float alarm_scale_of(const world& w, entity_id subject)
{
    float s = nation_scalar(w, subject, modifier_subject::alarm_toward, effect_target{}, 1.0f);
    if (!std::isfinite(s) || s < 0.0f)
        s = 1.0f;
    return s;
}

war_price_row priced(const sentiment_params& p, entity_id observer, sentiment_factor_kind kind,
                     float magnitude, bool borders, float alarm_scale, bool live)
{
    war_price_row r;
    r.observer  = observer;
    r.kind      = kind;
    r.magnitude = magnitude;
    const sentiment_factor& f = p.factors[static_cast<std::size_t>(kind)];
    // The fold's own arithmetic per dimension (sentiment.cpp: weight x magnitude,
    // Alarm also x the event's alarm scale); nothing when the gate drops it.
    r.access    = live ? f.access * magnitude : 0.0f;
    r.trust     = live ? f.trust * magnitude : 0.0f;
    r.alarm     = live ? f.alarm * magnitude * alarm_scale : 0.0f;
    r.borders   = borders;
    return r;
}
} // namespace

namespace
{
/// The declaration's price for @p subject declaring on @p leader, with
/// hostility_declared on every nation in @p across (ascending): one row for a
/// declaration, every member across for a join (BL-1706).
war_price price_rows(const world& w, const sentiment_params& p, entity_id subject, entity_id leader,
                     std::vector<entity_id> across, const std::vector<uint32_t>& goals)
{
    war_price out;
    out.valid          = true;
    out.justifications = war_justifications(w, p, subject, leader, goals);
    out.justified      = std::any_of(out.justifications.begin(), out.justifications.end(),
                                     [](bool b) { return b; });

    out.charged = sentiment_conduct_live(w);
    const float scale = alarm_scale_of(w, subject); // every row's subject is the declarer
    // hostility_declared: each nation across toward the declarer, once.
    std::sort(across.begin(), across.end());
    for (const entity_id x : across)
        out.rows.push_back(priced(p, x, sentiment_factor_kind::hostility_declared, 1.0f, false,
                                  scale, out.charged));

    // war_declared: every other nation toward the declarer.
    const std::set<entity_id> border = nations_bordering(w, subject, leader);
    const float goal_factor =
        1.0f + k_war_per_extra_goal * static_cast<float>(goals.size() - 1);
    std::vector<entity_id> others;
    others.reserve(w.nations.size());
    for (const auto& [nid, nc] : w.nations)
        if (nid != subject && !contains(across, nid) && !nc.ended) // BL-1562: an ended nation watches nothing
            others.push_back(nid);
    std::sort(others.begin(), others.end());
    for (const entity_id o : others)
    {
        const bool b = border.count(o) != 0;
        float m = goal_factor * (b ? k_war_border_multiplier : 1.0f);
        if (out.justified)
            m *= k_war_justified_multiplier;
        out.rows.push_back(priced(p, o, sentiment_factor_kind::war_declared, m, b, scale, out.charged));
    }
    for (const war_price_row& r : out.rows)
    {
        out.sum_access += r.access;
        out.sum_trust  += r.trust;
        out.sum_alarm  += r.alarm;
    }
    return out;
}
} // namespace

war_price declaration_price(const world& w, const sentiment_params& p, entity_id declarer,
                            entity_id defender, const std::vector<uint32_t>& goals)
{
    if (declare_check(w, declarer, defender, goals) != war_result::applied)
        return war_price{};
    return price_rows(w, p, declarer, defender, { defender }, goals);
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
    // BOTH ROWS (Ben, 2026-11-09; RELATIONS.md § 7): a declaration of war makes both
    // sides hostile, so the defender can push, intercept and fight from the first day
    // without answering. Answering only names its goals. (A bare `declare_hostile`
    // stays directed.)
    write_hostility(w, declarer, defender);
    write_hostility(w, defender, declarer);
    w.wars[{declarer, defender}] = std::move(rec);

    // Gathered, not written: the tick's one fold canonicalises it with the rest.
    for (const war_price_row& r : price.rows)
        queue_conduct(w, r.observer, declarer, r.kind, r.magnitude);

    // The public record (BL-1480, NR-1120).
    {
        const std::size_t n = w.wars.at({declarer, defender}).declarer_goals.size();
        log_war_event(w, declarer, defender,
                      nation_label(w, declarer) + " declared war on " + nation_label(w, defender),
                      std::to_string(n) + (n == 1 ? " province named as a goal" : " provinces named as goals")
                          + (price.justified ? "; justified" : ""));
    }
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
    // BL-1706: a goal is named once per side — a joiner of the defender's side may hold it.
    for (const uint32_t g : goals)
        if (war_goal_namer(it->second, 1, g) != null_entity)
            return war_result::rejected_state;

    bool distinct = true;
    write_hostility(w, defender, declarer); // standing since the declaration; kept idempotent
    it->second.defender_goals = sorted_goals(goals, distinct);
    it->second.answered       = true;
    return war_result::applied;
}


// ---------------------------------------------------------------------------
// War score, exhaustion and morale (BL-1428)
// ---------------------------------------------------------------------------

int war_score(const world& w, const war_record& war)
{
    // Occupation (Ben, 2026-10-31, NR-1103): each side normalised by ITS OWN goals —
    // the tile-weighted share of the declarer side's goals that side holds, less the
    // tile-weighted share of the defender side's (answered, joined) goals it holds.
    // Per SIDE (BL-1706, RELATIONS.md § Joining a war): any member's hold counts for
    // its side. A goal an in-play merge has removed weighs nothing.
    const auto held_share = [&w, &war](int side) {
        const std::vector<uint32_t> goals = war_side_goals(war, side);
        int64_t total = 0, held = 0;
        for (const uint32_t g : goals)
            if (const province* pr = w.provinces.find(g))
            {
                const int64_t n = static_cast<int64_t>(pr->tiles.size());
                total += n;
                if (war_side(war, province_holder_for(w, g)) == side)
                    held += n;
            }
        return total > 0 ? static_cast<double>(held) / static_cast<double>(total) : 0.0;
    };
    const double occupation = held_share(0) - held_share(1);
    // Battles: the strength each side destroyed — the other side's men lost.
    const int64_t lost0 = war_side_men_lost(war, 0), lost1 = war_side_men_lost(war, 1);
    const int64_t lost = lost0 + lost1;
    const double battles =
        lost > 0 ? static_cast<double>(lost1 - lost0) / static_cast<double>(lost) : 0.0;
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
        for (const war_member& m : war.joiners) // BL-1706
            if (m.nation == nation) e = std::max(e, m.exhaustion);
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
    for (const auto& [key, war] : w.wars)
    {
        parties.insert(war.declarer);
        parties.insert(war.defender);
        for (const war_member& m : war.joiners) parties.insert(m.nation); // BL-1706
    }
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
    std::vector<war_losses> losses;
    losses.reserve(battles.dispatches.size());
    for (const battle_dispatch& d : battles.dispatches) // battle order: sorted
        losses.push_back({ d.attacker, d.defender, d.attacker_men_lost, d.defender_men_lost });
    note_war_losses(w, losses);
}

void note_war_losses(world& w, const std::vector<war_losses>& losses)
{
    if (w.wars.empty() || losses.empty())
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
    // BL-1706: losses between any two nations across in a war count toward that war
    // and toward each member's own tally and exhaustion.
    const auto charge = [&](war_record& war, entity_id n, int lost) {
        int64_t* men = nullptr;
        float*   ex  = nullptr;
        if (n == war.declarer)      { men = &war.declarer_men_lost; ex = &war.declarer_exhaustion; }
        else if (n == war.defender) { men = &war.defender_men_lost; ex = &war.defender_exhaustion; }
        else
            for (war_member& m : war.joiners)
                if (m.nation == n) { men = &m.men_lost; ex = &m.exhaustion; break; }
        if (men == nullptr)
            return;
        *men += std::max(0, lost);
        exhaust(*ex, n, lost);
    };
    bool any = false;
    for (const war_losses& d : losses) // the caller's order: sorted
    {
        war_record* war = find_war_mut(w, d.a, d.b);
        if (war == nullptr)
            continue;
        any = true;
        // The declarer's tally first, as ever (the order the sums take is moot).
        if (d.b == war->declarer)
        {
            charge(*war, d.b, d.b_lost);
            charge(*war, d.a, d.a_lost);
        }
        else
        {
            charge(*war, d.a, d.a_lost);
            charge(*war, d.b, d.b_lost);
        }
    }
    if (any)
        apply_war_morale_ceiling(w);
}

void accumulate_war_field_days(world& w, int days)
{
    if (w.wars.empty() || days <= 0)
        return;
    // Per war, each member's units in the field: standing on the ground of a member
    // across, or fighting one in a live battle — as a share of its standing units
    // (BL-1706: per member, against every member across).
    std::map<entity_id, std::set<entity_id>> fighting; // unit -> the nations it is fighting
    for (const active_battle& b : w.battles)
    {
        if (b.state.end != campaign_battle_end::in_progress)
            continue;
        for (const entity_id u : b.attacker_units) fighting[u].insert(b.defender);
        for (const entity_id u : b.defender_units) fighting[u].insert(b.attacker);
    }
    // owner -> (war key, side) of each war it is a member of.
    std::map<entity_id, std::vector<std::pair<war_key_t, int>>> member_of;
    for (const auto& [key, war] : w.wars)
        for (const int s : { 0, 1 })
            for (const entity_id n : war_side_members(war, s))
                member_of[n].emplace_back(key, s);
    // (war key, owner) -> units in the field; owner -> units standing. Integer
    // counts, so the unordered walk cannot reach them.
    std::map<std::pair<war_key_t, entity_id>, int> in_field;
    std::map<entity_id, int> units_of;
    for (const auto& [uid, u] : w.units)
    {
        if (u.count <= 0)
            continue;
        ++units_of[u.owner];
        const auto mit = member_of.find(u.owner);
        if (mit == member_of.end())
            continue;
        const auto tn = w.tile_to_nation.find(u.position);
        const auto fit = fighting.find(uid);
        for (const auto& [key, side] : mit->second)
        {
            const war_record& war = w.wars.at(key);
            const bool abroad = tn != w.tile_to_nation.end() && war_side(war, tn->second) == 1 - side;
            bool fights = false;
            if (fit != fighting.end())
                for (const entity_id enemy : fit->second)
                    if (war_side(war, enemy) == 1 - side) { fights = true; break; }
            if (abroad || fights)
                ++in_field[{ key, u.owner }];
        }
    }
    const auto share = [&](const war_key_t& key, entity_id n) {
        const auto tot = units_of.find(n);
        const auto f = in_field.find({ key, n });
        if (tot == units_of.end() || tot->second <= 0 || f == in_field.end())
            return 0.0f;
        return static_cast<float>(f->second) / static_cast<float>(tot->second);
    };
    const float d = static_cast<float>(days);
    for (auto& [key, war] : w.wars) // ascending
    {
        war.declarer_field_days += d * share(key, war.declarer);
        war.defender_field_days += d * share(key, war.defender);
        for (war_member& m : war.joiners)
            m.field_days += d * share(key, m.nation);
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
        for (const auto& [key, war] : w.wars)
            for (const int s : { 0, 1 })
                for (const entity_id n : war_side_members(war, s))
                    at_war_nations.insert(n);
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
        // BL-1706: a member's provinces held by ANY member across.
        const auto held_across = [&](const war_record& war, entity_id owner, int side) {
            int n = 0;
            for (const entity_id x : war_side_members(war, 1 - side))
                n += held_by(owner, x);
            return n;
        };

        // The field days the quarter's days accumulated (`accumulate_war_field_days`)
        // are charged here and reset.
        for (auto& [key, war] : w.wars) // ascending (declarer, defender)
        {
            war.declarer_exhaustion = add_exhaustion(
                war.declarer_exhaustion,
                k_war_exhaustion_per_field_day * war.declarer_field_days
                    + k_war_exhaustion_per_held_province * static_cast<float>(held_across(war, war.declarer, 0)));
            war.defender_exhaustion = add_exhaustion(
                war.defender_exhaustion,
                k_war_exhaustion_per_field_day * war.defender_field_days
                    + k_war_exhaustion_per_held_province * static_cast<float>(held_across(war, war.defender, 1)));
            war.declarer_field_days = 0.0f;
            war.defender_field_days = 0.0f;
            // BL-1706: a joiner tires as a leader does, from ITS provinces any member
            // across holds.
            for (war_member& m : war.joiners)
            {
                const int held_n = held_across(war, m.nation, m.side);
                m.exhaustion = add_exhaustion(
                    m.exhaustion, k_war_exhaustion_per_field_day * m.field_days
                                      + k_war_exhaustion_per_held_province * static_cast<float>(held_n));
                m.field_days = 0.0f;
            }
        }
    }

    // BL-1706: a request lapses once its asker can no longer join (its war closing
    // erases it with the war).
    for (auto it = w.join_requests.begin(); it != w.join_requests.end();)
        it = check_join(w, it->first.first, it->first.second, it->second.side, it->second.goals, true)
                     != war_result::applied
                 ? w.join_requests.erase(it)
                 : std::next(it);

    // A nation in no open war sheds its residual.
    for (auto it = w.war_exhaustion_residual.begin(); it != w.war_exhaustion_residual.end();)
    {
        bool fighting_now = false;
        for (const auto& [key, war] : w.wars)
            if (war_side(war, it->first) >= 0) { fighting_now = true; break; }
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

/// Land provinces @p nation owns (politically): every land province whose
/// `tile_to_nation` plurality is @p nation. Counted over the WHOLE partition by the
/// territorial map (BL-1460), never over a list a nation keeps, so a cession at an
/// earlier peace — which moved the ground — counts here, and the last-province floor
/// holds across wars.
std::size_t provinces_owned(const world& w, entity_id nation)
{
    if (w.nations.find(nation) == w.nations.end())
        return 0;
    std::size_t n = 0;
    for (const province& pr : w.provinces.provinces) // ascending id
        if (province_kind_of(w, pr) == province_kind::land && province_owner(w, pr.id) == nation)
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

/// BL-1706: what a leader's capitulation closes the war with (RELATIONS.md § 7).
struct capitulation_info
{
    entity_id             gainer = null_entity; ///< The other side's leader.
    std::vector<uint32_t> ceded;                ///< Ascending, everything handed over.
    int                   score  = 0;           ///< `war_score` before the cession.
    entity_id             by     = null_entity; ///< The capitulating leader.
};

/// The nation that took the most of @p ceded_to's provinces from one loser — the
/// gainer of its elimination (ties to the lower id). @p ceded_to: namer -> count.
entity_id largest_gainer(const std::map<entity_id, std::size_t>& ceded_to)
{
    entity_id best = null_entity;
    std::size_t best_n = 0;
    for (const auto& [n, c] : ceded_to) // ascending: a strict '>' keeps the lower id
        if (c > best_n)
        {
            best   = n;
            best_n = c;
        }
    return best;
}

/// BL-1706: drop every goal no longer on the ground of a member across from its namer
/// (run when the membership changes: a joiner leaves, a leader's side passes on).
void prune_goals(const world& w, war_record& war)
{
    for (const int s : { 0, 1 })
    {
        const std::vector<entity_id> across = war_side_members(war, 1 - s);
        const auto stale = [&](uint32_t g) { return !contains(across, province_owner(w, g)); };
        std::vector<uint32_t>& lead = s == 0 ? war.declarer_goals : war.defender_goals;
        lead.erase(std::remove_if(lead.begin(), lead.end(), stale), lead.end());
        for (war_member& m : war.joiners)
            if (m.side == s)
                m.goals.erase(std::remove_if(m.goals.begin(), m.goals.end(), stale), m.goals.end());
    }
}

/// Every land unit of @p side_nation standing on ground any of @p across owns leaves
/// it by the retreat rule (BL-1502: occupation reaches past the goals).
void withdraw_from(world& w, entity_id side_nation, const std::vector<entity_id>& across)
{
    const auto& rows = unit_roster_table();
    std::vector<entity_id> leaving;
    for (const auto& [uid, u] : w.units) // collected; withdraw_units walks ascending
    {
        if (u.owner != side_nation || u.count <= 0 || u.position == null_entity || u.type >= rows.size()
            || (rows[u.type].domains & domain_bit(traversal_domain::land)) == 0)
            continue; // a fleet at sea stands on no one's ground
        const auto tn = w.tile_to_nation.find(u.position);
        if (tn != w.tile_to_nation.end() && contains(across, tn->second))
            leaving.push_back(uid);
    }
    if (leaving.empty())
        return;
    std::vector<uint32_t> theirs; // the provinces table is ascending
    for (const province& pr : w.provinces.provinces)
        if (contains(across, province_owner(w, pr.id)))
            theirs.push_back(pr.id);
    (void)withdraw_units(w, leaving, theirs, peace_day_stamp(w));
}

/// Closes the war under @p key on @p terms (already validated) for EVERY member
/// (BL-1706). One mutation. @p truce is false only for a war closed by a party's
/// elimination (NATIONS.md § Elimination: "No truce is written: there is nobody left
/// to declare"). @p cap: a leader capitulated (its cession already made).
void conclude_peace(world& w, const sentiment_params& p, war_key_t key, const peace_terms& terms,
                    bool imposed, bool truce = true, const capitulation_info* cap = nullptr)
{
    const war_record war = w.wars.at(key);
    const std::vector<entity_id> side0 = war_side_members(war, 0), side1 = war_side_members(war, 1);
    std::vector<entity_id> members = side0;
    members.insert(members.end(), side1.begin(), side1.end());
    const auto side_of = [&war](entity_id n) { return war_side(war, n); };
    const auto pairs = cross_pairs(war);

    war_history_entry h;
    h.declarer    = war.declarer;
    h.defender    = war.defender;
    h.opened_tick = war.opened_tick;
    h.closed_tick = w.current_econ_tick;
    h.final_score = cap != nullptr ? cap->score : war_score(w, war);

    std::vector<sentiment_event> events; // gathered below (queue_conduct), folded with the tick
    for (const auto& [a, b] : pairs) // every pair that stood across
    {
        events.push_back(sentiment_event{ a, b, sentiment_factor_kind::peace_concluded, 1.0f });
        events.push_back(sentiment_event{ b, a, sentiment_factor_kind::peace_concluded, 1.0f });
    }

    // A cession: each province passes from its owner to the member of the winning side
    // that NAMED it (BL-1706, RELATIONS.md § Joining a war), grouped by (owner, namer).
    std::map<entity_id, std::map<entity_id, std::size_t>> lost_to; // loser -> namer -> count
    std::map<entity_id, std::vector<uint32_t>> lost_provinces;     // loser -> provinces
    if (terms.kind == peace_kind::cession)
    {
        const int ws = war_side(war, terms.gainer);
        std::map<std::pair<entity_id, entity_id>, std::vector<uint32_t>> groups;
        for (const uint32_t pid : terms.provinces) // ascending
            groups[{ province_owner(w, pid), war_goal_namer(war, ws, pid) }].push_back(pid);
        for (const auto& [ft, provs] : groups)
        {
            cede_provinces(w, ft.first, ft.second, provs);
            events.push_back(sentiment_event{ ft.first, ft.second, sentiment_factor_kind::province_ceded,
                                              static_cast<float>(provs.size()) });
            lost_to[ft.first][ft.second] += provs.size();
            auto& lp = lost_provinces[ft.first];
            lp.insert(lp.end(), provs.begin(), provs.end());
        }
        h.end    = imposed ? war_end::imposed_cession : war_end::cession;
        h.gainer = terms.gainer;
        h.ceded  = terms.provinces;
    }
    else if (cap != nullptr)
    {
        h.end    = war_end::capitulation;
        h.gainer = cap->gainer;
        h.ceded  = cap->ceded;
    }
    else
        h.end = imposed ? war_end::imposed_white : war_end::white_peace;

    // AT ANY PEACE (BL-1460 fix round): every goal of THIS war that a member holds
    // against a member across and that was not ceded goes back to its owner — so at
    // peace no province stays held against its owner by a former enemy in the war. A
    // third nation's occupation, from its own war, is untouched. A ceded goal is its
    // holder's now (owner == holder), skipped by the test.
    {
        for (const entity_id m : members)
            if (const std::vector<uint32_t>* goals = war_member_goals(war, m))
                for (const uint32_t g : *goals)
                {
                    const entity_id owner  = province_owner(w, g);
                    const entity_id holder = province_holder_for(w, g);
                    const int so = side_of(owner), sh = side_of(holder);
                    if (owner != holder && so >= 0 && sh >= 0 && so != sh)
                        set_holder(w, g, owner);
                }
        // BL-1502: occupation reaches past the goals, so the walk does too. Every land unit
        // of a member left on the ground of a member across leaves it.
        for (const entity_id m : members)
            withdraw_from(w, m, side_of(m) == 0 ? side1 : side0);
    }

    // Every battle still being fought between a pair across ENDS with the peace, closed
    // as drawn: erased with no dispatch, no retreat and no holder write, so nothing a
    // peace settled is undone by a fight that outlived it.
    w.battles.erase(
        std::remove_if(w.battles.begin(), w.battles.end(),
                       [&](const active_battle& b) {
                           const int sa = side_of(b.attacker), sd = side_of(b.defender);
                           return sa >= 0 && sd >= 0 && sa != sd;
                       }),
        w.battles.end());

    // The record closes; every member's exhaustion carries out of it.
    w.wars.erase(key);
    for (const entity_id m : members)
    {
        float e = 0.0f;
        if (m == war.declarer) e = war.declarer_exhaustion;
        else if (m == war.defender) e = war.defender_exhaustion;
        else
            for (const war_member& j : war.joiners)
                if (j.nation == m) e = j.exhaustion;
        if (e >= k_war_exhaustion_epsilon)
        {
            float& r = w.war_exhaustion_residual[m];
            r = std::max(r, e);
        }
    }
    // Hostility clears for every pair across, unless another open war keeps it across.
    for (const auto& [a, b] : pairs)
        if (!at_war(w, a, b))
        {
            w.corp_hostile_pairs.erase({ a, b });
            w.corp_hostile_pairs.erase({ b, a });
        }
    // The public record (BL-1480, NR-1120), before the history entry moves out.
    {
        const char* how = h.end == war_end::white_peace   ? "ended in a white peace"
                        : h.end == war_end::cession       ? "ended in a peace by cession"
                        : h.end == war_end::imposed_white ? "ended in an imposed white peace"
                        : h.end == war_end::capitulation  ? "ended in a capitulation"
                                                          : "ended in an imposed cession";
        std::string consequence;
        if (!h.ceded.empty())
            consequence = std::to_string(h.ceded.size())
                        + (h.ceded.size() == 1 ? " province ceded to " : " provinces ceded to ")
                        + nation_label(w, h.gainer);
        if (cap != nullptr)
            consequence = nation_label(w, cap->by) + " capitulated"
                        + (consequence.empty() ? std::string() : "; " + consequence);
        std::vector<entity_id> parties{ war.declarer, war.defender };
        for (const war_member& j : war.joiners)
            parties.push_back(j.nation);
        log_war_event_all(w, parties,
                          "The war of " + nation_label(w, war.declarer) + " on " + nation_label(w, war.defender)
                              + " " + how,
                          consequence);
    }
    w.war_history.push_back(std::move(h));
    w.peace_offers.erase(key);
    for (const auto& [a, b] : pairs) // cooldowns and separate offers end with the war
    {
        w.peace_refusals.erase({ a, b });
        w.peace_refusals.erase({ b, a });
        w.separate_peace_offers.erase({ a, b });
        w.separate_peace_offers.erase({ b, a });
    }
    for (auto it = w.join_requests.begin(); it != w.join_requests.end();) // requests lapse
        it = it->first.first == key ? w.join_requests.erase(it) : std::next(it);
    for (auto it = w.join_refusals.begin(); it != w.join_refusals.end();)
        it = it->first.first == key ? w.join_refusals.erase(it) : std::next(it);
    // BL-1502 (MILITARY.md § Occupation): occupation clears at peace. A ceded province
    // keeps its new owner (done above); every other tile returns to its owner.
    clear_occupation_at_peace(w);
    if (truce)
        for (const auto& [a, b] : pairs)
            w.truces[canonical_pair(a, b)] = w.current_econ_tick + k_war_truce_ticks;
    for (const sentiment_event& e : events)
        queue_conduct(w, e.observer, e.subject, e.kind, e.magnitude);
    (void)p; // the weights are the fold's, read when the queue folds

    // A LAST PROVINCE CEDED ENDS THE LOSER (Ben, 2026-11-09; NATIONS.md § Elimination):
    // once the ground has moved, a loser left owning no land province is eliminated, to
    // the member that took the most from it.
    for (const auto& [loser, to] : lost_to)
        if (provinces_owned(w, loser) == 0)
        {
            std::vector<uint32_t> lp = lost_provinces.at(loser);
            std::sort(lp.begin(), lp.end());
            eliminate_nation(w, p, loser, largest_gainer(to), lp);
        }
}

/// BL-1706: @p nation, a JOINER, leaves the war at @p key — by a separate peace, its
/// capitulation or its end. Its goals go with it and the goals named on its ground
/// drop; its losses stay on its side's tally. Every goal held between it and a member
/// across returns to its owner, units on each other's ground withdraw, battles between
/// them end, hostility clears unless another war keeps the pair across, and (unless
/// @p ended) truces open and peace_concluded warms the rows. Logged when @p event is set.
void remove_member(world& w, const war_key_t& key, entity_id nation, bool ended,
                   const std::string& event, const std::string& consequence)
{
    war_record& war = w.wars.at(key);
    const int s = war_side(war, nation);
    const std::vector<entity_id> across = war_side_members(war, 1 - s);
    std::vector<uint32_t> goals_all = war_side_goals(war, 0); // before the prune
    {
        const std::vector<uint32_t> g1 = war_side_goals(war, 1);
        goals_all.insert(goals_all.end(), g1.begin(), g1.end());
    }
    for (auto it = war.joiners.begin(); it != war.joiners.end(); ++it)
        if (it->nation == nation)
        {
            war.departed_men_lost[s == 0 ? 0 : 1] += it->men_lost;
            if (!ended && it->exhaustion >= k_war_exhaustion_epsilon)
            {
                float& r = w.war_exhaustion_residual[nation];
                r = std::max(r, it->exhaustion);
            }
            war.joiners.erase(it);
            break;
        }
    prune_goals(w, war);

    for (const uint32_t g : goals_all)
    {
        const entity_id owner  = province_owner(w, g);
        const entity_id holder = province_holder_for(w, g);
        if ((owner == nation && contains(across, holder)) || (holder == nation && contains(across, owner)))
            set_holder(w, g, owner);
    }
    withdraw_from(w, nation, across);
    for (const entity_id x : across)
        withdraw_from(w, x, { nation });
    w.battles.erase(std::remove_if(w.battles.begin(), w.battles.end(),
                                   [&](const active_battle& b) {
                                       return (b.attacker == nation && contains(across, b.defender))
                                           || (b.defender == nation && contains(across, b.attacker));
                                   }),
                    w.battles.end());
    for (const entity_id x : across)
    {
        w.peace_refusals.erase({ nation, x });
        w.peace_refusals.erase({ x, nation });
        w.separate_peace_offers.erase({ nation, x });
        if (!at_war(w, nation, x))
        {
            w.corp_hostile_pairs.erase({ nation, x });
            w.corp_hostile_pairs.erase({ x, nation });
        }
    }
    clear_occupation_at_peace(w);
    if (ended)
        return;
    for (const entity_id x : across)
    {
        w.truces[canonical_pair(nation, x)] = w.current_econ_tick + k_war_truce_ticks;
        queue_conduct(w, nation, x, sentiment_factor_kind::peace_concluded, 1.0f);
        queue_conduct(w, x, nation, sentiment_factor_kind::peace_concluded, 1.0f);
    }
    if (!event.empty())
        log_war_event_all(w, { nation, war_leader(w.wars.at(key), 1 - s) }, event, consequence);
}

/// BL-1706 (RELATIONS.md § Joining a war): a leader whose nation ends hands its side
/// to the member that joined earliest. The war is re-keyed; its leaders' offer drops.
void promote_leader(world& w, const war_key_t& key, int side)
{
    war_record war = w.wars.at(key);
    const auto jit = std::find_if(war.joiners.begin(), war.joiners.end(),
                                  [side](const war_member& m) { return m.side == side; });
    const war_member j = *jit;
    war.joiners.erase(jit);
    if (side == 0)
    {
        war.departed_men_lost[0] += war.declarer_men_lost;
        war.declarer            = j.nation;
        war.declarer_goals      = j.goals;
        war.declarer_men_lost   = j.men_lost;
        war.declarer_exhaustion = j.exhaustion;
        war.declarer_field_days = j.field_days;
    }
    else
    {
        war.departed_men_lost[1] += war.defender_men_lost;
        war.defender            = j.nation;
        war.defender_goals      = j.goals;
        war.defender_men_lost   = j.men_lost;
        war.defender_exhaustion = j.exhaustion;
        war.defender_field_days = j.field_days;
        war.answered            = true;
    }
    const war_key_t nk{ war.declarer, war.defender };
    w.wars.erase(key);
    prune_goals(w, war);
    w.wars.emplace(nk, std::move(war));
    w.peace_offers.erase(key);
    for (auto it = w.separate_peace_offers.begin(); it != w.separate_peace_offers.end();)
        it = it->first.first == j.nation ? w.separate_peace_offers.erase(it) : std::next(it);
    const auto rekey = [&](auto& table) {
        std::vector<std::pair<war_join_key_t, typename std::decay_t<decltype(table)>::mapped_type>> moved;
        for (auto it = table.begin(); it != table.end();)
            if (it->first.first == key)
            {
                moved.emplace_back(war_join_key_t{ nk, it->first.second }, it->second);
                it = table.erase(it);
            }
            else
                ++it;
        for (auto& [k, v] : moved)
            table.emplace(k, std::move(v));
    };
    rekey(w.join_requests);
    rekey(w.join_refusals);
}

war_result pair_check(const world& w, entity_id a, entity_id b)
{
    if (a == b || !is_nation(w, a) || !is_nation(w, b))
        return war_result::rejected_invalid;
    if (!at_war(w, a, b))
        return war_result::rejected_state;
    return war_result::applied;
}

/// BL-1706: how two nations across stand for a peace: the war's two leaders, or a
/// joiner and the OTHER side's leader (a separate peace); anything else has no peace.
struct peace_pair
{
    const war_record* war = nullptr;
    war_key_t         key{};
    bool              leaders = false;
    entity_id         joiner  = null_entity;
    entity_id         leader  = null_entity;
};

peace_pair classify_peace(const world& w, entity_id a, entity_id b)
{
    peace_pair out;
    out.war = war_of_member(w, a, b, &out.key);
    if (out.war == nullptr)
        return out;
    const war_record& war = *out.war;
    if ((a == war.declarer && b == war.defender) || (a == war.defender && b == war.declarer))
        out.leaders = true;
    else if (b == war_leader(war, 1 - war_side(war, a)) && a != war.declarer && a != war.defender)
    {
        out.joiner = a;
        out.leader = b;
    }
    else if (a == war_leader(war, 1 - war_side(war, b)) && b != war.declarer && b != war.defender)
    {
        out.joiner = b;
        out.leader = a;
    }
    return out;
}

/// BL-1706: a separate peace's terms — white, or a cession of 1..3 goals the gainer
/// named on the other's ground that the gainer's side holds.
war_result check_separate_terms(const world& w, const war_record& war, entity_id joiner, entity_id leader,
                                const peace_terms& terms)
{
    if (terms.kind == peace_kind::white)
        return (terms.gainer == null_entity && terms.provinces.empty()) ? war_result::applied
                                                                        : war_result::rejected_invalid;
    if (terms.kind != peace_kind::cession)
        return war_result::rejected_invalid;
    if (terms.gainer != joiner && terms.gainer != leader)
        return war_result::rejected_invalid;
    if (terms.provinces.empty() || terms.provinces.size() > k_war_goals_max)
        return war_result::rejected_invalid;
    bool distinct = true;
    if (sorted_goals(terms.provinces, distinct) != terms.provinces || !distinct)
        return war_result::rejected_invalid;
    const entity_id other = terms.gainer == joiner ? leader : joiner;
    const std::vector<entity_id> side = war_side_members(war, war_side(war, terms.gainer));
    const std::vector<uint32_t>* goals = war_member_goals(war, terms.gainer);
    for (const uint32_t pid : terms.provinces)
    {
        if (goals == nullptr || !std::binary_search(goals->begin(), goals->end(), pid))
            return war_result::rejected_state; // only its own goals
        if (!contains(side, province_holder_for(w, pid)))
            return war_result::rejected_state; // only held ones
        if (province_owner(w, pid) != other)
            return war_result::rejected_state; // on the other's ground
    }
    return war_result::applied;
}

/// BL-1706: concludes a separate peace — the joiner alone leaves the war.
void conclude_separate_peace(world& w, const sentiment_params& p, const war_key_t& key, entity_id joiner,
                             entity_id leader, const peace_terms& terms)
{
    entity_id from = null_entity;
    if (terms.kind == peace_kind::cession)
    {
        from = terms.gainer == joiner ? leader : joiner;
        cede_provinces(w, from, terms.gainer, terms.provinces);
        queue_conduct(w, from, terms.gainer, sentiment_factor_kind::province_ceded,
                      static_cast<float>(terms.provinces.size()));
    }
    std::string consequence;
    if (terms.kind == peace_kind::cession)
        consequence = std::to_string(terms.provinces.size())
                    + (terms.provinces.size() == 1 ? " province ceded to " : " provinces ceded to ")
                    + nation_label(w, terms.gainer);
    remove_member(w, key, joiner, false,
                  nation_label(w, joiner) + " made a separate peace with " + nation_label(w, leader), consequence);
    if (from != null_entity && provinces_owned(w, from) == 0)
        eliminate_nation(w, p, from, terms.gainer, terms.provinces);
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
    // BL-1706: per SIDE — goals the winning side named, held by any of its members,
    // on the ground of any member of the losing side.
    const int ws = terms.gainer == war.declarer ? 0 : 1;
    const std::vector<uint32_t> goals = war_side_goals(war, ws);
    const std::vector<entity_id> winners = war_side_members(war, ws), losers = war_side_members(war, 1 - ws);
    for (const uint32_t pid : terms.provinces)
    {
        if (!std::binary_search(goals.begin(), goals.end(), pid))
            return war_result::rejected_state; // only goals
        if (!contains(winners, province_holder_for(w, pid)))
            return war_result::rejected_state; // only held ones
        if (!contains(losers, province_owner(w, pid)))
            return war_result::rejected_state; // still the loser's to cede
    }
    // A peace may cede the loser's LAST province (Ben, 2026-11-09, overturning NR-1102's
    // refusal): the loser's nation then ends (`eliminate_nation`).
    return war_result::applied;
}

bool nation_ended(const world& w, entity_id nation)
{
    const auto it = w.nations.find(nation);
    return it != w.nations.end() && it->second.ended;
}

void eliminate_nation(world& w, const sentiment_params& p, entity_id ended, entity_id gainer,
                      const std::vector<uint32_t>& ceded)
{
    const auto eit = w.nations.find(ended);
    if (eit == w.nations.end() || eit->second.ended || ended == gainer)
        return;
    const bool has_gainer = gainer != null_entity && w.nations.count(gainer) != 0;

    // 0. ITS LAST GROUND. A nation owns its coastal water as well as its land; with its
    //    last land province gone, every province it still OWNS passes to the conqueror
    //    by the cession path (so buildings, track and units follow). A tile it owns inside
    //    a province another nation owns (fix round, cold review) passes ALONE: owner, tile
    //    lists, deposit profile and track, with no holder write and no cession record, so
    //    the conqueror is never handed a third nation's province. The owners are read
    //    before anything moves. Ascending province, then ascending tile.
    std::vector<uint32_t> gained; // the provinces the gainer holds by this end: ceded + residual
    if (has_gainer)
    {
        std::set<uint32_t> residual;
        for (const entity_id t : eit->second.tiles)
            if (const auto o = w.tile_to_nation.find(t); o != w.tile_to_nation.end() && o->second == ended)
            {
                const uint32_t pid = w.provinces.province_of(t);
                if (pid != 0 && pid != no_province && province_owner(w, pid) == ended)
                    residual.insert(pid);
            }
        gained.assign(residual.begin(), residual.end());
        if (!gained.empty())
            cede_provinces(w, ended, gainer, gained);
        nation_component& loser = w.nations.at(ended);
        nation_component& to    = w.nations.at(gainer);
        std::vector<entity_id> loose;
        for (const entity_id t : loser.tiles)
            if (const auto o = w.tile_to_nation.find(t); o != w.tile_to_nation.end() && o->second == ended)
                loose.push_back(t);
        std::sort(loose.begin(), loose.end());
        loose.erase(std::unique(loose.begin(), loose.end()), loose.end());
        for (const entity_id t : loose)
        {
            w.set_tile_nation(t, gainer);
            to.tiles.push_back(t);
            if (const auto tc = w.tiles.find(t); tc != w.tiles.end())
                for (std::size_t r = 0; r < resource_count; ++r)
                {
                    loser.resource_abundance[r] = std::max(0.0f, loser.resource_abundance[r] - tc->second.resource_deposit[r]);
                    to.resource_abundance[r] += tc->second.resource_deposit[r];
                }
            if (const auto rt = w.rail_track.find(t); rt != w.rail_track.end() && rt->second == ended)
                rt->second = gainer;
        }
        loser.tiles.erase(std::remove_if(loser.tiles.begin(), loser.tiles.end(),
                                         [&](entity_id t) {
                                             const auto o = w.tile_to_nation.find(t);
                                             return o == w.tile_to_nation.end() || o->second != ended;
                                         }),
                          loser.tiles.end());
        // Whatever it still owns off its ground (nothing, on a generated world) passes too:
        // its remaining buildings, and its space assets (Ben, 2026-11-09), ascending id.
        to.state_assets.insert(to.state_assets.end(), loser.state_assets.begin(), loser.state_assets.end());
        loser.state_assets.clear();
        for (auto& [sid, sa] : w.space_assets)
            if (sa.owner == ended)
                sa.owner = gainer;
        gained.insert(gained.end(), ceded.begin(), ceded.end());
        std::sort(gained.begin(), gained.end());
        gained.erase(std::unique(gained.begin(), gained.end()), gained.end());
    }

    // 1. ITS WARS CLOSE: every other war it LEADS, as a white peace with no truce —
    //    unless its side has a joiner, to whom the side passes (BL-1706, the earliest
    //    to join); a war it JOINED goes on without it.
    {
        std::vector<war_key_t> keys;
        for (const auto& [key, war] : w.wars) // ascending
            if (war_side(war, ended) >= 0)
                keys.push_back(key);
        for (const auto& key : keys)
        {
            const auto wit = w.wars.find(key);
            if (wit == w.wars.end())
                continue;
            const int s = war_side(wit->second, ended);
            if (s < 0)
                continue;
            if (key.first == ended || key.second == ended)
            {
                const bool passes = std::any_of(wit->second.joiners.begin(), wit->second.joiners.end(),
                                                [s](const war_member& m) { return m.side == s; });
                if (passes)
                    promote_leader(w, key, s);
                else
                    conclude_peace(w, p, key, peace_terms{}, false, /*truce=*/false);
            }
            else
                remove_member(w, key, ended, /*ended=*/true, std::string(), std::string());
        }
    }

    // 2. ITS RELATIONS CLEAR: offers, hostility, friendship, embargo, its own laws (the
    //    tariffs it levied), truces and refusals naming it, and every sentiment row to or
    //    from it, with the conduct still queued about it.
    const auto names = [ended](const std::pair<entity_id, entity_id>& k) {
        return k.first == ended || k.second == ended;
    };
    const auto erase_pairs = [&](auto& table) {
        for (auto it = table.begin(); it != table.end();)
            it = names(*it) ? table.erase(it) : std::next(it);
    };
    const auto erase_keyed = [&](auto& table) {
        for (auto it = table.begin(); it != table.end();)
            it = names(it->first) ? table.erase(it) : std::next(it);
    };
    erase_pairs(w.corp_hostile_pairs);
    erase_pairs(w.corp_friend_pairs);
    erase_pairs(w.corp_friend_offers);
    erase_pairs(w.nation_embargoes);
    erase_keyed(w.peace_offers);
    erase_keyed(w.peace_refusals);
    erase_keyed(w.separate_peace_offers); // BL-1706
    for (auto it = w.join_requests.begin(); it != w.join_requests.end();)
        it = it->first.second == ended ? w.join_requests.erase(it) : std::next(it);
    for (auto it = w.join_refusals.begin(); it != w.join_refusals.end();)
        it = it->first.second == ended ? w.join_refusals.erase(it) : std::next(it);
    erase_keyed(w.truces);
    erase_keyed(w.sentiment.pairs);
    erase_keyed(w.sentiment.contributions);
    erase_keyed(w.sentiment_trade_landed);
    erase_keyed(w.sentiment_trade_streaks);
    w.sentiment_pending.erase(
        std::remove_if(w.sentiment_pending.begin(), w.sentiment_pending.end(),
                       [&](const sentiment_event& e) { return e.observer == ended || e.subject == ended; }),
        w.sentiment_pending.end());
    w.laws.erase(std::remove_if(w.laws.begin(), w.laws.end(),
                                [&](const law& l) { return l.enacting_nation == ended; }),
                 w.laws.end());
    if (auto rit = w.war_exhaustion_residual.find(ended); rit != w.war_exhaustion_residual.end())
        w.war_exhaustion_residual.erase(rit);

    // 3. ITS FORCE ENDS: units disband where they stand, with its wings, musters, front
    //    orders and any fight it is still in; its convoys and trades are cancelled.
    w.battles.erase(std::remove_if(w.battles.begin(), w.battles.end(),
                                   [&](const active_battle& b) { return b.attacker == ended || b.defender == ended; }),
                    w.battles.end());
    {
        std::vector<entity_id> gone;
        for (const auto& [uid, u] : w.units)
            if (u.owner == ended)
                gone.push_back(uid);
        std::sort(gone.begin(), gone.end());
        for (const entity_id uid : gone)
        {
            w.units.erase(uid);
            w.forget_air_unit(uid);
        }
    }
    for (auto it = w.wings.begin(); it != w.wings.end();)
        it = it->second.owner == ended ? w.wings.erase(it) : std::next(it);
    // The naval merge: its fleet ends with its army. Each of its ships leaves the world with
    // everyone aboard, and every side's sighting of it; its own sightings and its ship
    // designs clear (what the v88 loader refuses on an ended nation).
    for (auto it = w.ships.begin(); it != w.ships.end();)
        it = it->second.owner == ended ? w.ships.erase(it) : std::next(it);
    for (auto it = w.sea_sightings.begin(); it != w.sea_sightings.end();)
        it = (it->first == ended || w.ships.count(it->second) == 0) ? w.sea_sightings.erase(it) : std::next(it);
    w.nations.at(ended).ship_design_kit.clear();
    w.missile_magazines.erase(ended); // BL-1431: its magazine ends with it (its units' interceptors went with them)
    w.musters.erase(std::remove_if(w.musters.begin(), w.musters.end(),
                                   [&](const muster_order& m) { return m.owner == ended; }),
                    w.musters.end());
    w.front_orders.erase(std::remove_if(w.front_orders.begin(), w.front_orders.end(),
                                        [&](const front_order_entry& f) { return f.nation == ended; }),
                         w.front_orders.end());
    for (auto it = w.air_superiority.begin(); it != w.air_superiority.end();)
        it = it->first.second == ended ? w.air_superiority.erase(it) : std::next(it);
    for (auto it = w.air_revealed.begin(); it != w.air_revealed.end();)
        it = it->first == ended ? w.air_revealed.erase(it) : std::next(it);
    for (auto it = w.tile_occupier.begin(); it != w.tile_occupier.end();)
        it = it->second == ended ? w.tile_occupier.erase(it) : std::next(it);
    w.convoys.erase(std::remove_if(w.convoys.begin(), w.convoys.end(),
                                   [&](const convoy_component& c) { return c.corp == ended; }),
                    w.convoys.end());
    w.trades.erase(std::remove_if(w.trades.begin(), w.trades.end(),
                                  [&](const standing_trade& t) { return t.owner == ended; }),
                   w.trades.end());

    // 3b. NO THIRD PARTY STANDS AT PEACE ON THE NEW GROUND (fix round, cold review): with
    //    the ended nation's wars closed, a land unit of a nation NOT at war with the gainer
    //    left on a province the gainer just gained (the ceded and the residual ones)
    //    withdraws by the retreat rule, as a peace's own withdrawal does.
    if (has_gainer && !gained.empty())
    {
        const auto& rows = unit_roster_table();
        std::vector<entity_id> leaving;
        for (const auto& [uid, u] : w.units) // collected; withdraw_units walks ascending
        {
            if (u.owner == gainer || u.owner == ended || u.count <= 0 || u.position == null_entity
                || u.type >= rows.size() || (rows[u.type].domains & domain_bit(traversal_domain::land)) == 0)
                continue;
            if (at_war(w, u.owner, gainer))
                continue;
            const auto tn = w.tile_to_nation.find(u.position);
            if (tn == w.tile_to_nation.end() || tn->second != gainer)
                continue;
            const uint32_t pid = w.provinces.province_of(u.position);
            if (std::binary_search(gained.begin(), gained.end(), pid))
                leaving.push_back(uid);
        }
        if (!leaving.empty())
            (void)withdraw_units(w, leaving, gained, peace_day_stamp(w));
    }

    // 4. ITS TREASURY PASSES TO THE CONQUEROR: a positive balance is an actor-to-actor
    //    transfer that conserves; a debt is written off, not inherited. The receipt is
    //    BOOKED (fix round, cold review) through the refund mechanism (BL-1539): the
    //    gainer's next return lists it, so its net still equals its balance change. The
    //    ended nation's own unbooked amounts go with it: it files no return again.
    nation_component& nc = w.nations.at(ended);
    if (has_gainer && nc.treasury > 0.0f)
    {
        nation_component& to = w.nations.at(gainer);
        if (to.refund_unbooked == 0.0f)
            to.refund_opening = to.treasury;
        to.refund_unbooked += nc.treasury;
        to.treasury        += nc.treasury;
    }
    nc.treasury          = 0.0f;
    nc.trade_reserve     = 0.0f;
    nc.trade_points      = 0.0f;
    nc.refund_unbooked   = 0.0f;
    nc.refund_opening    = 0.0f;
    nc.research_unbooked = 0.0;
    nc.upkeep_unbooked   = 0.0;

    // 5. IT STAYS A RECORD: id, name and history kept, flagged ended. Its generated
    //    capital stays its identity; its live capital names no tile it owns.
    if (nc.generation_capital_tile == null_entity)
        nc.generation_capital_tile = nc.capital_tile;
    nc.capital_tile = null_entity;
    nc.ended        = true;

    invalidate_logistics_caches(w);
    reanchor_fronts(w);

    // The history log writes ONE entry, tagged by the ended nation; every nation reads
    // the log, so it is a public event.
    world_history_entry e;
    e.timestamp   = w.current_day_tick;
    e.topic       = history_topic::war;
    e.corp        = ended;
    e.event       = nation_label(w, ended) + " has ended";
    e.consequence = has_gainer ? "its last province passed to " + nation_label(w, gainer) : std::string();
    w.history_log.push_back(std::move(e));
}

bool peace_offer_cooling(const world& w, entity_id offerer, entity_id other)
{
    const auto it = w.peace_refusals.find({ offerer, other });
    return it != w.peace_refusals.end()
        && w.current_econ_tick - it->second < k_war_offer_cooldown_ticks;
}

war_result offer_peace(world& w, entity_id offerer, entity_id other, const peace_terms& terms)
{
    if (const war_result r = pair_check(w, offerer, other); r != war_result::applied)
        return r;
    const peace_pair pp = classify_peace(w, offerer, other);
    if (!pp.leaders && pp.joiner == null_entity)
        return war_result::rejected_state; // BL-1706: peace is between leaders, or a separate one
    const war_record& war = *pp.war;
    if (pp.leaders ? w.peace_offers.count(pp.key) != 0
                   : w.separate_peace_offers.count({ pp.joiner, pp.leader }) != 0)
        return war_result::rejected_state; // one pending offer per war (per joiner, separately)
    // A refused offer may not be repeated for four ticks (Ben, 2026-11-03, NR-1120).
    if (peace_offer_cooling(w, offerer, other))
        return war_result::rejected_state;
    if (const war_result r = pp.leaders ? check_peace_terms(w, war, terms)
                                        : check_separate_terms(w, war, pp.joiner, pp.leader, terms);
        r != war_result::applied)
        return r;
    peace_offer o;
    o.offerer      = offerer;
    o.terms        = terms;
    o.offered_tick = w.current_econ_tick;
    if (pp.leaders)
        w.peace_offers.emplace(pp.key, std::move(o));
    else
        w.separate_peace_offers.emplace(std::make_pair(pp.joiner, pp.leader), std::move(o));
    return war_result::applied;
}

war_result accept_peace(world& w, const sentiment_params& p, entity_id receiver, entity_id offerer)
{
    if (const war_result r = pair_check(w, receiver, offerer); r != war_result::applied)
        return r;
    const peace_pair pp = classify_peace(w, receiver, offerer);
    if (pp.leaders)
    {
        const auto oit = w.peace_offers.find(pp.key);
        if (oit == w.peace_offers.end() || oit->second.offerer != offerer)
            return war_result::rejected_state;
        const peace_terms terms = oit->second.terms;
        if (check_peace_terms(w, w.wars.at(pp.key), terms) != war_result::applied)
            return war_result::rejected_state; // the terms no longer hold; the offer stays
        conclude_peace(w, p, pp.key, terms, false);
        return war_result::applied;
    }
    if (pp.joiner == null_entity)
        return war_result::rejected_state;
    const auto oit = w.separate_peace_offers.find({ pp.joiner, pp.leader });
    if (oit == w.separate_peace_offers.end() || oit->second.offerer != offerer)
        return war_result::rejected_state;
    const peace_terms terms = oit->second.terms;
    if (check_separate_terms(w, *pp.war, pp.joiner, pp.leader, terms) != war_result::applied)
        return war_result::rejected_state;
    w.separate_peace_offers.erase(oit);
    conclude_separate_peace(w, p, pp.key, pp.joiner, pp.leader, terms);
    return war_result::applied;
}

war_result refuse_peace(world& w, entity_id receiver, entity_id offerer)
{
    if (const war_result r = pair_check(w, receiver, offerer); r != war_result::applied)
        return r;
    const peace_pair pp = classify_peace(w, receiver, offerer);
    if (pp.leaders)
    {
        const auto oit = w.peace_offers.find(pp.key);
        if (oit == w.peace_offers.end() || oit->second.offerer != offerer)
            return war_result::rejected_state;
        w.peace_offers.erase(oit);
    }
    else
    {
        if (pp.joiner == null_entity)
            return war_result::rejected_state;
        const auto oit = w.separate_peace_offers.find({ pp.joiner, pp.leader });
        if (oit == w.separate_peace_offers.end() || oit->second.offerer != offerer)
            return war_result::rejected_state;
        w.separate_peace_offers.erase(oit);
    }
    w.peace_refusals[{ offerer, receiver }] = w.current_econ_tick; // NR-1120: the cooldown starts
    return war_result::applied;
}

war_result impose_peace(world& w, const sentiment_params& p, entity_id imposer, entity_id other,
                        const peace_terms& terms)
{
    if (const war_result r = pair_check(w, imposer, other); r != war_result::applied)
        return r;
    const peace_pair pp = classify_peace(w, imposer, other);
    if (!pp.leaders)
        return war_result::rejected_state; // BL-1706: imposing is the leaders' only
    const war_record& war = *pp.war;
    const int score = war_score(w, war);
    const bool won = imposer == war.declarer ? score >= 100 : score <= -100;
    if (const war_result r = check_peace_terms(w, war, terms); r != war_result::applied)
        return r;
    if (!won)
        return war_result::rejected_state;
    conclude_peace(w, p, pp.key, terms, true);
    return war_result::applied;
}

// ---------------------------------------------------------------------------
// Joining (BL-1706, wars draw in allies; RELATIONS.md § Joining a war)
// ---------------------------------------------------------------------------

bool join_cooling(const world& w, const war_key_t& key, entity_id asker)
{
    const auto it = w.join_refusals.find({ key, asker });
    return it != w.join_refusals.end() && w.current_econ_tick - it->second < k_war_offer_cooldown_ticks;
}

war_result check_join(const world& w, const war_key_t& key, entity_id asker, uint8_t side,
                      const std::vector<uint32_t>& goals, bool allow_pending)
{
    if (!is_nation(w, asker) || side > 1)
        return war_result::rejected_invalid;
    const auto wit = w.wars.find(key);
    if (wit == w.wars.end())
        return war_result::rejected_state;
    const war_record& war = wit->second;
    if (war_side(war, asker) >= 0)
        return war_result::rejected_state; // a member at most once, never of both sides
    const std::vector<entity_id> across = war_side_members(war, 1 - side);
    const std::vector<entity_id> own    = war_side_members(war, side);
    // 1..3 distinct land provinces of members across.
    if (goals.empty() || goals.size() > k_war_goals_max)
        return war_result::rejected_invalid;
    bool distinct = true;
    (void)sorted_goals(goals, distinct);
    if (!distinct)
        return war_result::rejected_invalid;
    for (const uint32_t g : goals)
    {
        if (g == 0 || g == no_province)
            return war_result::rejected_invalid;
        const province* pr = w.provinces.find(g);
        if (pr == nullptr || province_kind_of(w, *pr) != province_kind::land
            || !contains(across, province_owner(w, g)))
            return war_result::rejected_invalid;
    }
    for (const uint32_t g : goals) // a goal is named once per side
        if (war_goal_namer(war, side, g) != null_entity)
            return war_result::rejected_state;
    for (const entity_id x : across)
        if (at_war(w, asker, x) || truce_between(w, asker, x))
            return war_result::rejected_state;
    for (const entity_id y : own) // never at war with its own side
        if (at_war(w, asker, y))
            return war_result::rejected_state;
    if (!allow_pending && w.join_requests.count({ key, asker }) != 0)
        return war_result::rejected_state;
    if (join_cooling(w, key, asker))
        return war_result::rejected_state;
    return war_result::applied;
}

war_price join_price(const world& w, const sentiment_params& p, const war_key_t& key, entity_id asker,
                     uint8_t side, const std::vector<uint32_t>& goals)
{
    if (check_join(w, key, asker, side, goals, true) != war_result::applied)
        return war_price{};
    const war_record& war = w.wars.at(key);
    return price_rows(w, p, asker, war_leader(war, 1 - side), war_side_members(war, 1 - side), goals);
}

war_result request_join_war(world& w, entity_id asker, const war_key_t& key, uint8_t side,
                            const std::vector<uint32_t>& goals)
{
    if (const war_result r = check_join(w, key, asker, side, goals); r != war_result::applied)
        return r;
    bool distinct = true;
    join_request req;
    req.side  = side;
    req.goals = sorted_goals(goals, distinct);
    req.tick  = w.current_econ_tick;
    w.join_requests[{ key, asker }] = std::move(req);
    return war_result::applied;
}

namespace
{
/// The first pending request of @p asker on a side @p leader leads, in war-key order.
std::map<war_join_key_t, join_request>::const_iterator
find_join_request(const world& w, entity_id leader, entity_id asker)
{
    for (auto it = w.join_requests.begin(); it != w.join_requests.end(); ++it)
    {
        if (it->first.second != asker)
            continue;
        const auto wit = w.wars.find(it->first.first);
        if (wit != w.wars.end() && war_leader(wit->second, it->second.side) == leader)
            return it;
    }
    return w.join_requests.end();
}
} // namespace

war_result accept_join(world& w, const sentiment_params& p, entity_id leader, entity_id asker)
{
    if (leader == asker || !is_nation(w, leader) || !is_nation(w, asker))
        return war_result::rejected_invalid;
    const auto rit = find_join_request(w, leader, asker);
    if (rit == w.join_requests.end())
        return war_result::rejected_state;
    const war_key_t key = rit->first.first;
    const join_request req = rit->second;
    if (check_join(w, key, asker, req.side, req.goals, true) != war_result::applied)
        return war_result::rejected_state; // no longer holds; the run_war_quarter sweep lapses it
    // The price is read BEFORE any write, so it is exactly `join_price`.
    const war_price price = join_price(w, p, key, asker, req.side, req.goals);

    // Validation is complete; everything below is unconditional.
    war_record& war = w.wars.at(key);
    const std::vector<entity_id> across = war_side_members(war, 1 - req.side);
    for (const entity_id x : across) // hostility both ways, as a declaration writes
    {
        write_hostility(w, asker, x);
        write_hostility(w, x, asker);
    }
    war_member m;
    m.nation      = asker;
    m.side        = req.side;
    m.goals       = req.goals;
    m.joined_tick = w.current_econ_tick;
    war.joiners.push_back(std::move(m));
    w.join_requests.erase({ key, asker });
    w.join_refusals.erase({ key, asker });
    for (const war_price_row& r : price.rows)
        queue_conduct(w, r.observer, asker, r.kind, r.magnitude);
    log_war_event_all(w, { asker, war.declarer, war.defender },
                      nation_label(w, asker) + " joined the war of " + nation_label(w, war.declarer) + " on "
                          + nation_label(w, war.defender),
                      "on the side of " + nation_label(w, leader) + (price.justified ? "; justified" : ""));
    return war_result::applied;
}

war_result refuse_join(world& w, entity_id leader, entity_id asker)
{
    if (leader == asker || !is_nation(w, leader) || !is_nation(w, asker))
        return war_result::rejected_invalid;
    const auto rit = find_join_request(w, leader, asker);
    if (rit == w.join_requests.end())
        return war_result::rejected_state;
    const war_join_key_t k = rit->first;
    w.join_requests.erase(rit);
    w.join_refusals[k] = w.current_econ_tick; // the cooldown starts
    return war_result::applied;
}

war_result capitulate(world& w, const sentiment_params& p, entity_id nation, const war_key_t& key)
{
    if (!is_nation(w, nation))
        return war_result::rejected_invalid;
    const auto wit = w.wars.find(key);
    if (wit == w.wars.end())
        return war_result::rejected_state;
    const war_record war = wit->second; // a copy: the cession below reads it as it stood
    const int s = war_side(war, nation);
    if (s < 0)
        return war_result::rejected_state;

    // Every goal the other side named on its ground, held or not, to its namer.
    std::vector<std::pair<entity_id, std::vector<uint32_t>>> handed;
    for (const entity_id m : war_side_members(war, 1 - s))
    {
        std::vector<uint32_t> mine;
        if (const std::vector<uint32_t>* goals = war_member_goals(war, m))
            for (const uint32_t g : *goals)
                if (province_owner(w, g) == nation)
                    mine.push_back(g);
        if (!mine.empty())
            handed.emplace_back(m, std::move(mine));
    }
    const int score = war_score(w, war);

    // Validation is complete; everything below is unconditional.
    std::vector<uint32_t> all;
    std::map<entity_id, std::size_t> to;
    for (const auto& [namer, provs] : handed)
    {
        cede_provinces(w, nation, namer, provs);
        queue_conduct(w, nation, namer, sentiment_factor_kind::province_ceded, static_cast<float>(provs.size()));
        all.insert(all.end(), provs.begin(), provs.end());
        to[namer] += provs.size();
    }
    std::sort(all.begin(), all.end());

    if (nation == war.declarer || nation == war.defender)
    {
        capitulation_info cap;
        cap.gainer = war_leader(war, 1 - s);
        cap.ceded  = all;
        cap.score  = score;
        cap.by     = nation;
        conclude_peace(w, p, key, peace_terms{}, false, true, &cap);
    }
    else
    {
        std::string consequence;
        if (!all.empty())
            consequence = std::to_string(all.size()) + (all.size() == 1 ? " province ceded" : " provinces ceded");
        remove_member(w, key, nation, false,
                      nation_label(w, nation) + " capitulated in the war of " + nation_label(w, war.declarer)
                          + " on " + nation_label(w, war.defender),
                      consequence);
    }
    if (!all.empty() && provinces_owned(w, nation) == 0)
        eliminate_nation(w, p, nation, largest_gainer(to), all);
    return war_result::applied;
}

namespace
{
/// BL-1460 (NR-1102): where a nation whose capital was ceded puts its capital — the
/// tile of the largest population centre on its remaining land provinces (ties to
/// the lower province id, then the lower centre id). Read AFTER the ground moved.
entity_id capital_after_cession(const world& w, entity_id nation)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return null_entity;
    const auto owns = [&](entity_id t) {
        const auto it = w.tile_to_nation.find(t);
        return it != w.tile_to_nation.end() && it->second == nation;
    };
    // The nation's land provinces, by the territorial map.
    std::set<uint32_t> candidates;
    {
        std::set<uint32_t> touched;
        for (const entity_id t : nit->second.tiles)
            if (owns(t))
                if (const uint32_t p = w.provinces.province_of(t); p != 0)
                    touched.insert(p);
        for (const uint32_t p : touched)
            if (province_kind_of(w, p) == province_kind::land && province_owner(w, p) == nation)
                candidates.insert(p);
    }
    if (candidates.empty())
        return null_entity;
    // BL-1602 (MILITARY.md § Space, "A war spans every body"): a nation holding ground
    // on more than one body keeps its capital on the body it stood on while any land
    // province remains there; a base elsewhere is the capital of ITS body already
    // (NATIONS.md § Off-world bases). Only when none remains does the capital leave
    // the body. With every candidate on one body this filters nothing.
    if (const auto ct = w.tiles.find(nit->second.capital_tile); ct != w.tiles.end())
    {
        std::set<uint32_t> same_body;
        for (const uint32_t p : candidates)
            if (const province* pr = w.provinces.find(p); pr != nullptr && pr->body == ct->second.body)
                same_body.insert(p);
        if (!same_body.empty())
            candidates = std::move(same_body);
    }
    // Each candidate's largest centre on the nation's own ground: (population, centre
    // id, tile). Strictly larger wins; a tie keeps the lower centre id, whatever the
    // unordered store's walk order.
    struct best_centre { int population = -1; entity_id centre = null_entity; entity_id tile = null_entity; };
    std::map<uint32_t, best_centre> largest;
    for (const auto& [cid, pc] : w.population_centres)
    {
        const auto tit = w.population_centre_tile.find(cid);
        if (tit == w.population_centre_tile.end() || !owns(tit->second))
            continue;
        const uint32_t p = w.provinces.province_of(tit->second);
        if (candidates.count(p) == 0)
            continue;
        best_centre& b = largest[p];
        if (pc.population > b.population || (pc.population == b.population && cid < b.centre))
            b = best_centre{ pc.population, cid, tit->second };
    }
    // The province with the largest centre, ties to the lower province id (the map
    // walks ascending, and only a strictly larger centre replaces the best).
    uint32_t  best_p = 0;
    int       best_pop = -1;
    entity_id best_tile = null_entity;
    for (const auto& [p, b] : largest)
        if (b.population > best_pop)
        {
            best_p    = p;
            best_pop  = b.population;
            best_tile = b.tile;
        }
    if (best_tile != null_entity)
        return best_tile;
    // No centre stands on any of its provinces (a hand-built fixture): the lowest
    // province id, at its lowest tile the nation owns.
    (void)best_p;
    const province* pr = w.provinces.find(*candidates.begin());
    if (pr == nullptr)
        return null_entity;
    for (const entity_id t : pr->tiles) // ascending
        if (owns(t))
            return t;
    return null_entity;
}

} // namespace

void cede_provinces(world& w, entity_id from, entity_id to, const std::vector<uint32_t>& provinces_in)
{
    std::vector<uint32_t> provinces = provinces_in;
    std::sort(provinces.begin(), provinces.end());
    provinces.erase(std::unique(provinces.begin(), provinces.end()), provinces.end());
    // BL-1460 (RELATIONS.md § 7, "Cession moves the territory"): the holder (already
    // `to` for a legal cession) and the LOST GROUND record, then the ground itself.
    for (const uint32_t pid : provinces)
    {
        set_holder(w, pid, to);
        w.province_cessions.push_back(province_cession{ from, to, pid, w.current_econ_tick });
    }
    const auto fit = w.nations.find(from);
    const auto tit_n = w.nations.find(to);
    if (fit == w.nations.end() || tit_n == w.nations.end() || from == to)
        return;
    nation_component& loser  = fit->second;
    nation_component& gainer = tit_n->second;

    // THE OLD MAP FIRST (fix round, cold review): `reanchor_fronts` remaps a front key
    // only against the fronts cached before the territory moved. A load drops that
    // cache, so without this a cession right after a load would drop the assignments
    // and orders an unbroken run remaps — the saved, hashed state would diverge. Built
    // here from the map as it stands, before any tile moves, the old fronts exist on
    // both paths (a no-op when the cache is already current).
    reanchor_fronts(w);

    // The loser's free logistics hub, read while its capital is still its own: the
    // one `capital_free_source` names.
    entity_id free_hub = null_entity;
    for (const entity_id bid : loser.state_assets)
        if (capital_free_source(w, bid))
        {
            free_hub = bid;
            break;
        }

    // 1. THE GROUND. Every tile of each ceded province the loser owns passes to the
    //    gainer, through the one owner setter (which bumps the catchment and front
    //    stamps). Tiles a third nation owns stay its own. Ascending province, then
    //    ascending tile: the order is a property of the ids.
    std::vector<entity_id> moved;
    for (const uint32_t pid : provinces)
        if (const province* pr = w.provinces.find(pid))
            for (const entity_id t : pr->tiles)
                if (const auto it = w.tile_to_nation.find(t);
                    it != w.tile_to_nation.end() && it->second == from)
                {
                    w.set_tile_nation(t, to);
                    moved.push_back(t);
                }
    std::sort(moved.begin(), moved.end());
    if (moved.empty())
        return;
    // BL-1504: a defend order ends with its post's ground. The rest of the order's checks run
    // on the next military day; this one is an invariant the save reader holds it to.
    for (auto& [uid, u] : w.units)
        if (u.defend_province != no_province && std::binary_search(moved.begin(), moved.end(), u.defend_post))
        {
            u.defend_province = no_province;
            u.defend_post     = null_entity;
        }
    const auto was_ceded = [&](entity_id t) { return std::binary_search(moved.begin(), moved.end(), t); };

    // 2. THE NATION'S TILE LIST AND ITS DEPOSIT PROFILE follow: the loser keeps its
    //    list's order less the ceded tiles; the gainer appends them ascending. The
    //    Pass-3 profile moves by each tile's deposits (the loser's never below 0).
    loser.tiles.erase(std::remove_if(loser.tiles.begin(), loser.tiles.end(), was_ceded), loser.tiles.end());
    for (const entity_id t : moved)
    {
        gainer.tiles.push_back(t);
        if (const auto tc = w.tiles.find(t); tc != w.tiles.end())
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                loser.resource_abundance[r]  = std::max(0.0f, loser.resource_abundance[r] - tc->second.resource_deposit[r]);
                gainer.resource_abundance[r] += tc->second.resource_deposit[r];
            }
    }

    // 3. A CEDED CAPITAL MOVES (NR-1102): to the remaining province with the largest
    //    population centre, ties to the lower id; its free hub moves with it — unless a
    //    running hub of the loser already stands there (that one is now the free
    //    source, and the old one passes with its ground).
    bool hub_moves = false;
    entity_id new_capital = null_entity;
    if (loser.capital_tile != null_entity && was_ceded(loser.capital_tile))
    {
        new_capital = capital_after_cession(w, from);
        if (new_capital != null_entity)
        {
            // Generation's capital stays the identity (nation_preference): stamp it
            // first where a fixture never did, then move the live capital.
            if (loser.generation_capital_tile == null_entity)
                loser.generation_capital_tile = loser.capital_tile;
            loser.capital_tile = new_capital;
            if (free_hub != null_entity)
            {
                bool already = false;
                for (const entity_id bid : loser.state_assets)
                    if (const auto bit = w.buildings.find(bid);
                        bid != free_hub && bit != w.buildings.end() && bit->second.tile == new_capital
                        && bit->second.type == building_type::inland_logistics_hub
                        && bit->second.ticks_remaining <= 0 && !bit->second.decommissioned)
                        already = true;
                const auto ct = w.tiles.find(new_capital);
                hub_moves = !already && ct != w.tiles.end() && !is_water(ct->second.substrate);
                if (hub_moves)
                    w.buildings.at(free_hub).tile = new_capital;
            }
        }
    }

    // 4. THE BUILDINGS ON IT change owner with the ground (NATIONS.md: a nation owns
    //    every building on its ground): out of the loser's list in its order, onto
    //    the end of the gainer's in that same order.
    {
        std::vector<entity_id> keep, pass;
        keep.reserve(loser.state_assets.size());
        for (const entity_id bid : loser.state_assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit != w.buildings.end() && was_ceded(bit->second.tile))
                pass.push_back(bid);
            else
                keep.push_back(bid);
        }
        loser.state_assets = std::move(keep);
        gainer.state_assets.insert(gainer.state_assets.end(), pass.begin(), pass.end());
    }

    // 5. ITS TRACK follows its ground, as the fold hands track to its ground nation.
    for (const entity_id t : moved)
        if (const auto rt = w.rail_track.find(t); rt != w.rail_track.end() && rt->second == from)
            rt->second = to;

    // 6. A muster the loser was raising at a base that passed is dropped: the base is
    //    no longer its own (the queue's own rule for a base that is gone).
    w.musters.erase(std::remove_if(w.musters.begin(), w.musters.end(),
                                   [&](const muster_order& m) {
                                       return m.owner == from && w.buildings.count(m.base) != 0
                                           && owner_nation_of(w, m.base) == to;
                                   }),
                    w.musters.end());
    // A unit of the loser raised at such a base no longer records it (null = "no
    // muster base recorded", never orphaned): the base is another nation's now, and
    // its demolition there must not disband the loser's unit. Each write reads only
    // its own unit, so the unordered walk cannot reach the result.
    for (auto& [uid, u] : w.units)
        if (u.owner == from && u.muster_base != null_entity && w.buildings.count(u.muster_base) != 0
            && owner_nation_of(w, u.muster_base) == to)
            u.muster_base = null_entity;
    // An air wing of the loser based at an airbase that passed REBASES to the loser's
    // nearest own airbase (hex distance from the lost base, ties to the lower building
    // id; an airbase on another body counts as farther than any on this one). With no
    // airbase left it is DISBANDED, as the upkeep pass disbands a wing whose base is
    // gone (MILITARY.md § Upkeep, the orphan rule). Wings walk ascending id.
    {
        std::vector<entity_id> own_bases; // ascending
        for (const entity_id bid : loser.state_assets)
            if (const auto bit = w.buildings.find(bid);
                bit != w.buildings.end() && bit->second.type == building_type::airbase)
                own_bases.push_back(bid);
        std::sort(own_bases.begin(), own_bases.end());
        std::vector<entity_id> disband;
        for (auto& [wid, wg] : w.wings)
        {
            if (wg.owner != from || w.buildings.count(wg.airbase) == 0
                || owner_nation_of(w, wg.airbase) != to)
                continue;
            const entity_id lost_tile = w.buildings.at(wg.airbase).tile;
            entity_id best = null_entity;
            int best_d = std::numeric_limits<int>::max();
            for (const entity_id bid : own_bases) // ascending: a tie keeps the lower id
            {
                const int d0 = air_tile_distance(w, lost_tile, w.buildings.at(bid).tile);
                const int d  = d0 < 0 ? std::numeric_limits<int>::max() - 1 : d0;
                if (d < best_d)
                {
                    best_d = d;
                    best   = bid;
                }
            }
            if (best != null_entity)
                wg.airbase = best;
            else
                disband.push_back(wid);
        }
        for (const entity_id wid : disband)
            w.wings.erase(wid);
    }

    // 7. Every derived answer that read the old map: the logistics caches (reach,
    //    anchors, the power grid, LP fields) drop here; the market catchment and the
    //    fronts re-derive off the stamps `set_tile_nation` bumped.
    invalidate_logistics_caches(w);

    // 8. THE LOSER'S UNITS ON CEDED GROUND WITHDRAW by the retreat rule.
    std::vector<entity_id> leaving;
    for (const auto& [uid, u] : w.units) // collected, then walked ascending by withdraw_units
        if (u.owner == from && u.count > 0 && was_ceded(u.position))
            leaving.push_back(uid);
    (void)withdraw_units(w, leaving, provinces, peace_day_stamp(w));

    // 9. Fronts re-anchor onto the new border now, so a save taken before the next
    //    tick holds no assignment keyed on ground its unit's nation no longer holds.
    reanchor_fronts(w);
}
