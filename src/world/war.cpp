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

#include <limits>
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <unordered_map>

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

    out.charged = sentiment_conduct_live(w);
    const float scale = alarm_scale_of(w, declarer); // every row's subject is the declarer
    // hostility_declared: the defender toward the declarer, once.
    out.rows.push_back(priced(p, defender, sentiment_factor_kind::hostility_declared, 1.0f, false,
                              scale, out.charged));

    // war_declared: every other nation toward the declarer.
    const std::set<entity_id> border = nations_bordering(w, declarer, defender);
    const float goal_factor =
        1.0f + k_war_per_extra_goal * static_cast<float>(goals.size() - 1);
    std::vector<entity_id> others;
    others.reserve(w.nations.size());
    for (const auto& [nid, nc] : w.nations)
        if (nid != declarer && nid != defender && !nc.ended) // BL-1562: an ended nation watches nothing
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
    // the tile-weighted share of the declarer's goals the declarer holds, less the
    // tile-weighted share of the defender's (answered) goals the defender holds. An
    // unanswered war's second term is 0. A goal an in-play merge has removed weighs
    // nothing.
    const auto held_share = [&w](const std::vector<uint32_t>& goals, entity_id side) {
        int64_t total = 0, held = 0;
        for (const uint32_t g : goals)
            if (const province* pr = w.provinces.find(g))
            {
                const int64_t n = static_cast<int64_t>(pr->tiles.size());
                total += n;
                if (province_holder_for(w, g) == side)
                    held += n;
            }
        return total > 0 ? static_cast<double>(held) / static_cast<double>(total) : 0.0;
    };
    const double occupation = held_share(war.declarer_goals, war.declarer)
                            - held_share(war.defender_goals, war.defender);
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

/// Closes the war under @p key on @p terms (already validated). One mutation. @p truce
/// is false only for a war closed by a party's elimination (NATIONS.md § Elimination:
/// "No truce is written: there is nobody left to declare").
void conclude_peace(world& w, const sentiment_params& p, std::pair<entity_id, entity_id> key,
                    const peace_terms& terms, bool imposed, bool truce = true)
{
    const war_record war = w.wars.at(key);
    war_history_entry h;
    h.declarer    = war.declarer;
    h.defender    = war.defender;
    h.opened_tick = war.opened_tick;
    h.closed_tick = w.current_econ_tick;
    h.final_score = war_score(w, war);

    std::vector<sentiment_event> events; // gathered below (queue_conduct), folded with the tick
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
        h.end = imposed ? war_end::imposed_white : war_end::white_peace;

    // AT ANY PEACE (BL-1460 fix round): every goal of THIS war that one party holds
    // against the other and that was not ceded goes back to its owner — so at peace no
    // province stays held against its owner by a former party to the war. Only where owner
    // and holder are this war's two parties: a third nation's occupation, from its own war,
    // is untouched. A ceded goal is its holder's now (owner == holder), skipped by the test.
    {
        const auto party = [&](entity_id n) { return n == war.declarer || n == war.defender; };
        const auto& rows = unit_roster_table();
        for (const std::vector<uint32_t>* goals : { &war.declarer_goals, &war.defender_goals })
            for (const uint32_t g : *goals)
            {
                const entity_id owner  = province_owner(w, g);
                const entity_id holder = province_holder_for(w, g);
                if (owner != holder && party(owner) && party(holder))
                {
                    set_holder(w, g, owner);
                }
            }
        // BL-1502: occupation reaches past the goals, so the walk does too. Every land unit of
        // either party left on the other's ground leaves it, whichever province it is in, by
        // the same walk, until it stands outside the other's provinces.
        for (const entity_id side : { war.declarer, war.defender })
        {
            const entity_id other = side == war.declarer ? war.defender : war.declarer;
            std::vector<entity_id> leaving;
            for (const auto& [uid, u] : w.units) // collected; withdraw_units walks ascending
            {
                if (u.owner != side || u.count <= 0 || u.position == null_entity || u.type >= rows.size()
                    || (rows[u.type].domains & domain_bit(traversal_domain::land)) == 0)
                    continue; // a fleet at sea stands on no one's ground
                const auto tn = w.tile_to_nation.find(u.position);
                if (tn != w.tile_to_nation.end() && tn->second == other)
                    leaving.push_back(uid);
            }
            if (leaving.empty())
                continue;
            std::vector<uint32_t> theirs; // the provinces table is ascending
            for (const province& pr : w.provinces.provinces)
                if (province_owner(w, pr.id) == other)
                    theirs.push_back(pr.id);
            (void)withdraw_units(w, leaving, theirs, peace_day_stamp(w));
        }
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
    // The public record (BL-1480, NR-1120), before the history entry moves out.
    {
        const char* how = h.end == war_end::white_peace   ? "ended in a white peace"
                        : h.end == war_end::cession       ? "ended in a peace by cession"
                        : h.end == war_end::imposed_white ? "ended in an imposed white peace"
                                                          : "ended in an imposed cession";
        std::string consequence;
        if (!h.ceded.empty())
            consequence = std::to_string(h.ceded.size())
                        + (h.ceded.size() == 1 ? " province ceded to " : " provinces ceded to ")
                        + nation_label(w, h.gainer);
        log_war_event(w, war.declarer, war.defender,
                      "The war of " + nation_label(w, war.declarer) + " on " + nation_label(w, war.defender)
                          + " " + how,
                      consequence);
    }
    w.war_history.push_back(std::move(h));
    w.peace_offers.erase(key);
    w.peace_refusals.erase({ war.declarer, war.defender }); // cooldowns end with the war
    w.peace_refusals.erase({ war.defender, war.declarer });
    w.wars.erase(key);
    // BL-1502 (MILITARY.md § Occupation): occupation clears at peace. A ceded province
    // keeps its new owner (done above); every other tile returns to its owner.
    clear_occupation_at_peace(w);
    if (truce)
        w.truces[canonical_pair(war.declarer, war.defender)] = w.current_econ_tick + k_war_truce_ticks;
    for (const sentiment_event& e : events)
        queue_conduct(w, e.observer, e.subject, e.kind, e.magnitude);
    (void)p; // the weights are the fold's, read when the queue folds

    // A LAST PROVINCE CEDED ENDS THE LOSER (Ben, 2026-11-09; NATIONS.md § Elimination):
    // once the ground has moved, a loser left owning no land province is eliminated.
    if (terms.kind == peace_kind::cession)
    {
        const entity_id loser = terms.gainer == war.declarer ? war.defender : war.declarer;
        if (provinces_owned(w, loser) == 0)
            eliminate_nation(w, p, loser, terms.gainer, terms.provinces);
    }
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

    // 1. ITS WARS CLOSE: every other war it is party to, as a white peace with no truce.
    {
        std::vector<std::pair<entity_id, entity_id>> keys;
        for (const auto& [key, war] : w.wars) // ascending
            if (key.first == ended || key.second == ended)
                keys.push_back(key);
        for (const auto& key : keys)
            if (w.wars.count(key) != 0)
                conclude_peace(w, p, key, peace_terms{}, false, /*truce=*/false);
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
    const war_record& war = *war_between(w, offerer, other);
    const auto key = war_key(war);
    if (w.peace_offers.count(key) != 0)
        return war_result::rejected_state; // one pending offer per war
    // A refused offer may not be repeated for four ticks (Ben, 2026-11-03, NR-1120).
    if (peace_offer_cooling(w, offerer, other))
        return war_result::rejected_state;
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
    w.peace_refusals[{ offerer, receiver }] = w.current_econ_tick; // NR-1120: the cooldown starts
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
