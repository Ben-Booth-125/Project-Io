#include "world/history_pattern_detectors.hpp"

#include "world/settlement.hpp"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <set>
#include <utility>

// history_pattern_detectors.cpp — the five detectors (BL-1286). See the header for the
// rulings each reading follows. Moved here from tools/verify/history_patterns.cpp
// so the gate and the nation card read ONE implementation.

namespace {

using K = lapse_event_kind;
inline bool is(const lapse_event& e, K k) { return e.kind == static_cast<uint8_t>(k); }

/// Column-wrapped grid distance squared.
int64_t dist2(const region& a, const region& b, int gw)
{
    int dx = std::abs(a.col - b.col);
    if (gw > 0 && dx > gw / 2) dx = gw - dx;
    const int dy = a.row - b.row;
    return static_cast<int64_t>(dx) * dx + static_cast<int64_t>(dy) * dy;
}

/// Shares (per mille of held regions) of every polity at one step.
std::map<int, int> shares_at(const era_timelapse& t, const timelapse_step& st)
{
    int64_t total = 0;
    for (int i = 0; i < st.sample_count; ++i)
        total += t.samples[static_cast<std::size_t>(st.first_sample + i)].regions;
    std::map<int, int> out;
    if (total <= 0) return out;
    for (int i = 0; i < st.sample_count; ++i)
    {
        const polity_sample& s = t.samples[static_cast<std::size_t>(st.first_sample + i)];
        out[s.polity] = static_cast<int>(static_cast<int64_t>(s.regions) * 1000 / total);
    }
    return out;
}

void sorted_unique(std::vector<int32_t>& v)
{
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
}

// --- rise and fall -------------------------------------------------------------
void read_rise_and_fall(const era_timelapse& t, history_pattern_reading& r)
{
    if (t.steps.empty()) return;
    // The front-runner: the largest realm at the first recorded step at or
    // after 200 BCE (the last step when the record closes before it). Ties on
    // the lower polity id (the map walks ascending; strictly greater wins).
    std::size_t front_step = t.steps.size() - 1;
    for (std::size_t si = 0; si < t.steps.size(); ++si)
        if (t.steps[si].year >= hp_rf_front_runner_year) { front_step = si; break; }
    int best = -1, best_q = -1;
    for (const auto& kv : shares_at(t, t.steps[front_step]))
        if (kv.second > best_q) { best_q = kv.second; best = kv.first; }
    if (best < 0) return;
    r.rf_polity  = best;
    r.rf_front_q = best_q;
    // Its peak over the whole span, first step that reaches it.
    int peak_step = -1;
    for (std::size_t si = 0; si < t.steps.size(); ++si)
    {
        const auto sh = shares_at(t, t.steps[si]);
        const auto it = sh.find(best);
        if (it != sh.end() && it->second > r.rf_peak_q) { r.rf_peak_q = it->second; peak_step = static_cast<int>(si); }
    }
    if (peak_step < 0) return;
    r.rf_peak_year = t.steps[static_cast<std::size_t>(peak_step)].year;
    // The fall: the first later step at which its share is at most half the peak.
    int fall_year = 0;
    bool fell = false;
    for (std::size_t si = static_cast<std::size_t>(peak_step) + 1; si < t.steps.size() && !fell; ++si)
    {
        const auto sh = shares_at(t, t.steps[si]);
        const auto it = sh.find(best);
        const int  q  = it == sh.end() ? 0 : it->second;
        if (q * 2 <= r.rf_peak_q) { fall_year = t.steps[si].year; fell = true; }
    }
    r.rf_fall_years = fell ? fall_year - r.rf_peak_year : -1;
    const int fall_end = fell ? fall_year : t.start_year + t.years;
    for (const lapse_event& e : t.events)
        if ((is(e, K::broke_away) || is(e, K::schism)) && e.other == best
            && e.year > r.rf_peak_year && e.year <= fall_end)
            ++r.rf_breakaways;
    r.rf_hit = r.rf_peak_year <= hp_rf_peak_by_year
            && r.rf_peak_q >= hp_rf_floor_q && r.rf_peak_q < hp_rf_hegemony_q
            && r.rf_fall_years >= 0 && r.rf_fall_years <= hp_rf_fall_window_y;
    if (r.rf_hit)
    {
        r.rf_polities.push_back(best);
        r.grants.push_back({best, r.rf_peak_year, static_cast<uint8_t>(history_pattern::rise_and_fall)});
    }
}

// --- late culture --------------------------------------------------------------

/// Polity -> founding culture, read off the seat its first appearance names.
std::map<int, int> polity_cultures(const std::vector<const era_timelapse*>& recs, const settlement_state& ss)
{
    std::map<int, int> out;
    for (const era_timelapse* t : recs)
        for (const lapse_event& e : t->events)
        {
            if (!(is(e, K::founded) || is(e, K::broke_away) || is(e, K::schism) || is(e, K::inherited))) continue;
            if (e.polity == lapse_event_none || out.count(e.polity)) continue;
            if (e.region < ss.regions.size()) out[e.polity] = ss.regions[e.region].founding_culture;
        }
    return out;
}

void read_late_culture(const era_timelapse& emp, const era_timelapse& exp, const settlement_state& ss, int gw,
                       const std::map<int, int>& pc, history_pattern_reading& r)
{
    // The Culture round's close is the Empires record's opening year: ground
    // founded after it (the founding schedule's later arrivals) was not there.
    const bool have_close = !emp.steps.empty() || emp.years > 0;
    const int64_t close_year = emp.start_year;
    std::map<int, std::vector<int>> by_culture;
    for (std::size_t i = 0; i < ss.regions.size(); ++i)
    {
        const region& g = ss.regions[i];
        if (g.founding_culture < 0 || g.domain != region_domain::land) continue;
        if (have_close && g.founded_year > close_year) continue;
        by_culture[g.founding_culture].push_back(static_cast<int>(i));
    }
    if (by_culture.size() < 2) return;
    // Isolation: the smallest gap from any region of the culture to any foreign
    // one, at the close. A GEOMETRIC PROXY for "most remote by reach" (NR-1034):
    // the record carries no reach field to read.
    std::vector<std::pair<int64_t, int>> gaps;
    for (const auto& kv : by_culture)
    {
        int64_t g = INT64_MAX;
        for (const int a : kv.second)
            for (const auto& other : by_culture)
            {
                if (other.first == kv.first) continue;
                for (const int b : other.second)
                    g = std::min(g, dist2(ss.regions[static_cast<std::size_t>(a)],
                                          ss.regions[static_cast<std::size_t>(b)], gw));
            }
        gaps.push_back({g, kv.first});
    }
    std::sort(gaps.begin(), gaps.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second; });
    const int c = gaps[0].second;
    r.lc_culture    = c;
    r.lc_gap        = static_cast<int>(std::min<int64_t>(gaps[0].first, INT_MAX));
    r.lc_second_gap = static_cast<int>(std::min<int64_t>(gaps[1].first, INT_MAX));
    // Lag: the culture's polities' mean materials band at the Empires close vs the world's.
    if (!emp.steps.empty())
    {
        const timelapse_step& last = emp.steps.back();
        int64_t cs = 0, cn = 0, ws = 0, wn = 0;
        for (int i = 0; i < last.sample_count; ++i)
        {
            const polity_sample& s = emp.samples[static_cast<std::size_t>(last.first_sample + i)];
            ws += s.cap_materials; ++wn;
            const auto it = pc.find(s.polity);
            if (it != pc.end() && it->second == c) { cs += s.cap_materials; ++cn; }
        }
        if (cn > 0 && wn > 0 && ws > 0) r.lc_lag_x100 = static_cast<int>(cs * 100 * wn / (cn * ws));
    }
    // Colonised: a polity of the culture bound or bought by a polity of another.
    std::vector<history_pattern_reading::grant> bound;
    for (const lapse_event& e : exp.events)
    {
        if (!(is(e, K::subject_bound) || is(e, K::province_bought))) continue;
        const auto nat = pc.find(e.polity), ovr = pc.find(e.other);
        const int nc = nat == pc.end() ? -1 : nat->second, oc = ovr == pc.end() ? -2 : ovr->second;
        if (nc == c && oc != c)
        {
            ++r.lc_bound;
            bound.push_back({static_cast<int32_t>(e.polity), e.year,
                             static_cast<uint8_t>(history_pattern::late_culture)});
        }
    }
    r.lc_hit = r.lc_bound > 0 && r.lc_lag_x100 >= 0 && r.lc_lag_x100 <= hp_lc_lag_x100;
    if (r.lc_hit)
        for (const auto& g : bound) { r.lc_polities.push_back(g.polity); r.grants.push_back(g); }
    sorted_unique(r.lc_polities);
}

// --- world war -----------------------------------------------------------------
void read_world_war(const era_timelapse& t, history_pattern_reading& r)
{
    if (t.steps.empty()) return;
    // The top powers: the most populous at the first step at or after the
    // window's open (ties on the lower polity id).
    const timelapse_step* open = nullptr;
    for (const timelapse_step& st : t.steps)
        if (st.year >= hp_ww_window_start) { open = &st; break; }
    if (open == nullptr || open->year > hp_ww_window_end) return;
    std::vector<std::pair<int64_t, int>> pops;
    for (int i = 0; i < open->sample_count; ++i)
    {
        const polity_sample& s = t.samples[static_cast<std::size_t>(open->first_sample + i)];
        pops.push_back({s.population, s.polity});
    }
    std::sort(pops.begin(), pops.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second; });
    std::set<int> top;
    for (std::size_t i = 0; i < pops.size() && i < static_cast<std::size_t>(hp_ww_top_k); ++i) top.insert(pops[i].second);
    std::map<int, int> touched; // polity -> its first conflict moment's year
    for (const lapse_event& e : t.events)
    {
        if (e.year < hp_ww_window_start || e.year > hp_ww_window_end) continue;
        if (!(is(e, K::seat_captured) || is(e, K::realm_ended) || is(e, K::sea_leg_campaign) || is(e, K::treaty_broken)))
            continue;
        ++r.ww_moments;
        if (top.count(e.polity)) touched.emplace(e.polity, e.year);
        if (top.count(e.other))  touched.emplace(e.other, e.year);
    }
    r.ww_belligerents = static_cast<int>(touched.size());
    r.ww_hit = r.ww_belligerents >= hp_ww_min_belligerents;
    if (r.ww_hit)
        for (const auto& kv : touched)
        {
            r.ww_polities.push_back(kv.first);
            r.grants.push_back({kv.first, kv.second, static_cast<uint8_t>(history_pattern::world_war)});
        }
}

// --- civil war / revolution ----------------------------------------------------

/// Regions @p polity held at the first recorded step at or after @p year.
int regions_after(const era_timelapse& t, int polity, int year)
{
    for (const timelapse_step& st : t.steps)
    {
        if (st.year < year) continue;
        for (int i = 0; i < st.sample_count; ++i)
        {
            const polity_sample& s = t.samples[static_cast<std::size_t>(st.first_sample + i)];
            if (s.polity == polity) return s.regions;
        }
        return 0;
    }
    return 0;
}

/// Splits followed by their halves fighting. @p must_take_parent reads the
/// revolution proxy (the successor takes a seat of, or ends, its parent; no
/// size test). Returns (fights, splits); the fighting pairs go to @p named.
std::pair<int, int> read_split_wars(const era_timelapse& t, bool must_take_parent,
                                    std::vector<history_pattern_reading::grant>* named)
{
    int fights = 0, splits = 0;
    for (std::size_t i = 0; i < t.events.size(); ++i)
    {
        const lapse_event& s = t.events[i];
        if (!(is(s, K::broke_away) || is(s, K::schism))) continue;
        if (s.polity == lapse_event_none || s.other == lapse_event_none) continue;
        if (!must_take_parent)
        {
            // A HALF, not a city state walking off: the successor must hold a
            // real share of what the parent kept, read at the next step.
            const int child = regions_after(t, s.polity, s.year), parent = regions_after(t, s.other, s.year);
            if (child < hp_cw_min_regions || parent <= 0
                || static_cast<int64_t>(child) * 1000 < static_cast<int64_t>(parent) * hp_cw_min_half_q) continue;
        }
        ++splits;
        bool fought = false;
        int  fought_year = s.year;
        for (std::size_t j = i + 1; j < t.events.size() && !fought; ++j)
        {
            const lapse_event& e = t.events[j];
            fought_year = e.year;
            if (e.year > s.year + hp_cw_window_y) break;
            // seat_captured: polity = the taker, other = the loser.
            // realm_ended:   polity = the DEAD,  other = the killer.
            const bool child_on_parent = e.polity == s.polity && e.other == s.other;
            const bool parent_on_child = e.polity == s.other && e.other == s.polity;
            if (is(e, K::seat_captured))
                fought = must_take_parent ? child_on_parent : (child_on_parent || parent_on_child);
            else if (is(e, K::realm_ended))
                fought = must_take_parent ? parent_on_child /* the parent died at the child's hand */
                                          : (child_on_parent || parent_on_child);
        }
        if (fought)
        {
            ++fights;
            if (named != nullptr)
            {
                const uint8_t cw = static_cast<uint8_t>(history_pattern::civil_war);
                named->push_back({s.polity, fought_year, cw});
                named->push_back({s.other, fought_year, cw});
            }
        }
    }
    return {fights, splits};
}

} // namespace

history_pattern_reading detect_history_patterns(const era_timelapse& empires,
                                                const era_timelapse& exploration,
                                                const era_timelapse& industrialisation,
                                                const settlement_state& settlement,
                                                int gw)
{
    history_pattern_reading r;
    const std::map<int, int> pc = polity_cultures({&empires, &exploration, &industrialisation}, settlement);
    read_rise_and_fall(empires, r);
    read_late_culture(empires, exploration, settlement, gw, pc, r);
    read_world_war(industrialisation, r);
    std::vector<history_pattern_reading::grant> cw_named;
    const auto cw = read_split_wars(empires, false, &cw_named);
    r.cw_count = cw.first; r.cw_splits = cw.second;
    for (const auto& g : cw_named) { r.cw_polities.push_back(g.polity); r.grants.push_back(g); }
    sorted_unique(r.cw_polities);
    // REVOLUTION: "the takeover is recorded as a revolution event" — and no
    // such event kind exists on the record yet (BL-1289 owns it), so the
    // detector names nothing and `rv_count` is 0 by reading, not by fiat. The
    // proxy below is a reading for the gate, never a hit.
    r.rv_proxy = read_split_wars(industrialisation, true, nullptr).first;
    return r;
}

std::vector<uint8_t> fold_history_pattern_tags(const history_pattern_reading& reading,
                                               const era_timelapse& empires,
                                               const era_timelapse& exploration,
                                               const era_timelapse& industrialisation,
                                               const std::vector<int32_t>& nation_polity,
                                               const std::vector<int32_t>& absorbed_first,
                                               const std::vector<int32_t>& absorbed_count,
                                               const std::vector<int32_t>& absorbed,
                                               int* unfolded)
{
    // The grants in date order (stable: ties keep the detectors' order, which
    // is itself fixed), applied before the first event dated after them.
    std::vector<history_pattern_reading::grant> grants = reading.grants;
    std::stable_sort(grants.begin(), grants.end(),
                     [](const history_pattern_reading::grant& x, const history_pattern_reading::grant& y) {
                         return x.year < y.year; });
    std::map<int32_t, uint8_t> tags;   // polity -> the tags it holds now
    std::map<int32_t, uint8_t> passed; // polity -> the tags it handed on (to a killer or a successor)
    std::size_t gi = 0;
    const auto grant_through = [&](int64_t year) {
        for (; gi < grants.size() && grants[gi].year <= year; ++gi)
            if (grants[gi].polity >= 0)
                tags[grants[gi].polity] |= history_pattern_bit(static_cast<history_pattern>(grants[gi].pattern));
    };
    const auto hand_on = [&](int32_t from, int32_t to) {
        if (from < 0 || to < 0 || from == to) return;
        const auto it = tags.find(from);
        if (it == tags.end() || it->second == 0) return;
        const uint8_t m = it->second; // copied before `tags[to]` may rehash the map
        tags[to] |= m;
        passed[from] |= m;
    };
    for (const era_timelapse* t : {&empires, &exploration, &industrialisation})
        for (const lapse_event& e : t->events)
        {
            grant_through(e.year);
            const int32_t pol = e.polity == lapse_event_none ? -1 : static_cast<int32_t>(e.polity);
            const int32_t oth = e.other  == lapse_event_none ? -1 : static_cast<int32_t>(e.other);
            // A successor carries its parent line's tags: polity = successor, other = parent.
            if (is(e, K::broke_away) || is(e, K::schism) || is(e, K::seceded_at_close))
                hand_on(oth, pol);
            // A realm that ends passes its tags to whoever ended it: polity = the dead,
            // other = the killer (realm_ended) or the absorber (absorbed_at_close).
            else if (is(e, K::realm_ended) || is(e, K::absorbed_at_close))
                hand_on(pol, oth);
        }
    grant_through(INT64_MAX);

    const std::size_t n = nation_polity.size();
    std::vector<uint8_t> out(n, 0);
    std::set<int32_t> folded;
    const auto take = [&](int32_t q, std::size_t k) {
        if (q < 0) return;
        folded.insert(q);
        const auto it = tags.find(q);
        if (it != tags.end()) out[k] |= it->second;
    };
    for (std::size_t k = 0; k < n; ++k)
    {
        take(nation_polity[k], k);
        if (k >= absorbed_first.size() || k >= absorbed_count.size()) continue;
        const int64_t first = absorbed_first[k], count = absorbed_count[k];
        if (first < 0 || count < 0 || first + count > static_cast<int64_t>(absorbed.size())) continue;
        for (int64_t a = first; a < first + count; ++a) take(absorbed[static_cast<std::size_t>(a)], k);
    }
    if (unfolded != nullptr)
    {
        // A tag is lost only where its holder is no nation, was folded into
        // none, and handed it to nobody -- a realm that ended with no killer.
        int miss = 0;
        for (const auto& kv : tags)
        {
            if (folded.count(kv.first)) continue;
            const auto pit = passed.find(kv.first);
            const uint8_t lost = static_cast<uint8_t>(kv.second & ~(pit == passed.end() ? 0 : pit->second));
            for (int b = 0; b < static_cast<int>(history_pattern::count); ++b)
                miss += (lost >> b) & 1;
        }
        *unfolded = miss;
    }
    return out;
}
