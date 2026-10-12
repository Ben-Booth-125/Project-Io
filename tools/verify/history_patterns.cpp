// ---------------------------------------------------------------------------
// history_patterns — the five history-pattern detectors, read unforced
// (BL-1286 HISTORY_PATTERN_DETECTORS; GENERATION_STRATEGY.md § History patterns)
// ---------------------------------------------------------------------------
// MEASURE FIRST. Builds each seed's shipped world and reads, off the recorded
// history alone (the four era_timelapse records + the post-sim settlement),
// how near each of the five patterns already comes WITHOUT any target built:
//
//   RF  The rise and fall (Empires, always; BL-1287 — the RULED predicate,
//       CIVILISATION.md § How an empire actually falls). The realm largest by
//       share of held regions at 200 BCE: its share then, its peak share over
//       the span and the year of it (by 400 CE), its margin over the next realm
//       at that step, the seats it took on the way up, how long until its share
//       halved (within 300 years) and the share it fell to, and the successors
//       that broke away from it in between (at least one). The peak must clear
//       the floor `rise_fall_floor_pm` and stay under hegemony (BL-224, 500‰).
//       Its constants are the sim's own (history_sim.hpp `rise_fall_*`).
//   LC  The late culture (Culture -> Exploration, always). The founding culture
//       most isolated by grid gap (the smallest column-wrapped distance from
//       any of its regions to any foreign-founded region — a GEOMETRIC PROXY
//       for "most remote by reach", NR-1034), its materials lag at the Empires
//       close, and whether any polity of it was bound or bought in Exploration.
//   WW  The world war (Industrialisation, always). The largest number of the
//       six most populous realms touched by conflict moments (seat captured,
//       realm ended, wet campaign, treaty broken) inside one 30-year window,
//       and the span's deepest drop in total population.
//   CW  Civil war (Empires, lean). A successor breaks away holding at least a
//       quarter of what its parent kept (and 3+ regions), and it and its parent
//       then take seats from, or end, each other within 50 years.
//   RV  Revolution (Industrialisation, lean). A successor or schism in the span
//       that then takes its parent's capital or ends it.
//
// Beside them, BL-1278's measure-first: the natural nation count, the living
// polities at the last close, and how many nations hold a specialist (on a
// budget world, the PLANNED charters the carve ledger records — the specialists
// themselves are chartered after this build, in finish_campaign_world).
//
// Every predicate threshold here but RF's is PROVISIONAL — a reading aid for the
// design session, not a ruling. The raw readings are printed beside each verdict so a
// different threshold can be read off the same run. Exits 0 always: this is a
// census, not a gate (the gates are BL-1287/1288/1289's, built on these).
//
//   bash tools/verify/build_lua_harness.sh history_patterns
//   build_gen/verify/history_patterns.exe [--seeds 0,11,46] [--csv out.csv]

#include "harness_params.hpp"
#include "world/components.hpp"
#include "world/era_minus_one.hpp"
#include "world/era_timelapse.hpp"
#include "world/hard_coded_world.hpp"
#include "world/world.hpp"
#include "world/works_roster.hpp"
#include "world/world_gen_config.hpp"
#include "scripting/lua_state.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using K = lapse_event_kind;
inline bool is(const lapse_event& e, K k) { return e.kind == static_cast<uint8_t>(k); }

std::vector<uint32_t> library_seeds(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    std::vector<uint32_t> out;
    std::size_t pos = text.find("\"seeds\"");
    if (pos == std::string::npos) return out;
    const std::string key = "\"seed\"";
    while ((pos = text.find(key, pos)) != std::string::npos)
    {
        pos += key.size();
        std::size_t p = pos;
        while (p < text.size() && (text[p] == ' ' || text[p] == ':' || text[p] == '\t')) ++p;
        if (p < text.size() && text[p] >= '0' && text[p] <= '9')
            out.push_back(static_cast<uint32_t>(std::strtoul(text.c_str() + p, nullptr, 10)));
    }
    return out;
}

// --- provisional thresholds (reading aids, not rulings) -----------------------
// RF reads the RULED constants (history_sim.hpp `rise_fall_*`, BL-1287), so the
// detector and the sim's backstop judge one predicate; it is a gate, not a reading aid.
constexpr int    hegemony_q         = 500;  ///< BL-224, history_sim_harness
constexpr int    lc_lag_x100        = 85;   ///< culture materials mean <= 85% of world mean
constexpr int    ww_window_y        = 30;
constexpr int    ww_top_k           = 6;
constexpr int    ww_min_belligerents= 4;    ///< of the six most populous realms
constexpr int    cw_window_y        = 50;

struct row
{
    uint32_t seed = 0;
    double   build_s = 0;
    // RF
    int rf_polity = -1, rf_start_q = 0, rf_peak_q = 0, rf_margin_x100 = 0, rf_peak_year = 0, rf_peak_frac_pm = 0;
    int rf_captures = 0, rf_fall_years = -1, rf_fall_q = -1, rf_breakaways = 0;
    int rf_rise_level = 0, rf_fall_level = 0; ///< the levels the sim pressed (its recorded causes)
    bool rf_ended = false, rf_hit = false;
    // LC
    int lc_culture = -1, lc_gap = 0, lc_second_gap = 0, lc_regions = 0, lc_lag_x100 = -1;
    int lc_bound = 0, lc_cultures_bound = 0, lc_cultures = 0, lc_polities = 0;
    bool lc_hit = false;
    // WW
    int ww_belligerents = 0, ww_window_start = 0, ww_pop_drop_pm = 0, ww_moments = 0;
    bool ww_hit = false;
    // CW / RV
    int cw_count = 0, cw_breakaways = 0, rv_count = 0, rv_splits = 0;
    // BL-1278
    int nations = 0, nations_with_specialist = 0, specialists = 0, living_polities = 0;
};

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
    for (int i = 0; i < st.sample_count; ++i) total += t.samples[static_cast<std::size_t>(st.first_sample + i)].regions;
    std::map<int, int> out;
    if (total <= 0) return out;
    for (int i = 0; i < st.sample_count; ++i)
    {
        const polity_sample& s = t.samples[static_cast<std::size_t>(st.first_sample + i)];
        out[s.polity] = static_cast<int>(static_cast<int64_t>(s.regions) * 1000 / total);
    }
    return out;
}

void read_rise_and_fall(const era_timelapse& t, row& r)
{
    if (t.steps.empty()) return;
    // THE FRONT-RUNNER: the realm with the largest share at the step at
    // 200 BCE (the first recorded step at or after it), ties to the lower id.
    std::size_t pick = t.steps.size();
    for (std::size_t si = 0; si < t.steps.size(); ++si)
        if (t.steps[si].year >= rise_fall_pick_year) { pick = si; break; }
    if (pick == t.steps.size()) return;
    int best = -1, best0 = -1;
    for (const auto& kv : shares_at(t, t.steps[pick]))
        if (kv.second > best0) { best0 = kv.second; best = kv.first; }
    if (best < 0) return;
    r.rf_polity  = best;
    r.rf_start_q = best0;
    // ITS PEAK: its largest share at any step of the span (the first step
    // that reaches it), so a peak "by 400 CE" means it never stood higher after.
    int best_q = -1, best_step = -1;
    for (std::size_t si = 0; si < t.steps.size(); ++si)
    {
        const auto sh = shares_at(t, t.steps[si]);
        const auto it = sh.find(best);
        if (it != sh.end() && it->second > best_q) { best_q = it->second; best_step = static_cast<int>(si); }
    }
    if (best_step < 0) return;
    const timelapse_step& peak = t.steps[static_cast<std::size_t>(best_step)];
    int second = 0;
    for (const auto& kv : shares_at(t, peak))
        if (kv.first != best && kv.second > second) second = kv.second;
    r.rf_peak_q       = best_q;
    r.rf_margin_x100  = second > 0 ? best_q * 100 / second : 9999;
    r.rf_peak_year    = peak.year;
    r.rf_peak_frac_pm = t.years > 0 ? static_cast<int>(static_cast<int64_t>(peak.year - t.start_year) * 1000 / t.years) : 0;
    // The fall: the first later step at which its share is at most half the peak.
    int fall_year = 0;
    bool fell = false;
    for (std::size_t si = static_cast<std::size_t>(best_step) + 1; si < t.steps.size() && !fell; ++si)
    {
        const auto sh = shares_at(t, t.steps[si]);
        const auto it = sh.find(best);
        const int q   = it == sh.end() ? 0 : it->second;
        if (q * 2 <= best_q) { fall_year = t.steps[si].year; fell = true; r.rf_fall_q = q; }
    }
    r.rf_fall_years = fell ? fall_year - peak.year : -1;
    const int fall_end = fell ? fall_year : t.start_year + t.years;
    for (const lapse_event& e : t.events)
    {
        if (is(e, K::seat_captured) && e.polity == best && e.year <= peak.year) ++r.rf_captures;
        if ((is(e, K::broke_away) || is(e, K::schism)) && e.other == best && e.year > peak.year && e.year <= fall_end)
            ++r.rf_breakaways;
        if (is(e, K::realm_ended) && e.polity == best) r.rf_ended = true;
        if (is(e, K::rise_pressed) && e.polity == best) r.rf_rise_level = std::max<int>(r.rf_rise_level, e.other);
        if (is(e, K::overextended) && e.polity == best) r.rf_fall_level = std::max<int>(r.rf_fall_level, e.other);
    }
    // THE RULING (CIVILISATION.md § How an empire actually falls, Ben
    // 2026-10-11/12), clause for clause: the front-runner at 200 BCE peaks by
    // 400 CE at or above the floor and under the hegemony cap, and its share
    // halves within 300 years of the peak, with successors breaking away.
    r.rf_hit = r.rf_peak_year <= rise_fall_peak_by_year
            && r.rf_peak_q >= rise_fall_floor_pm && r.rf_peak_q < rise_fall_hegemony_pm
            && r.rf_fall_years >= 0 && r.rf_fall_years <= rise_fall_window_years
            && r.rf_breakaways >= 1;
}

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
                       const std::map<int, int>& pc, row& r)
{
    std::map<int, std::vector<int>> by_culture;
    for (std::size_t i = 0; i < ss.regions.size(); ++i)
        if (ss.regions[i].founding_culture >= 0 && ss.regions[i].domain == region_domain::land)
            by_culture[ss.regions[i].founding_culture].push_back(static_cast<int>(i));
    r.lc_cultures = static_cast<int>(by_culture.size());
    if (by_culture.size() < 2) return;
    // Isolation: the smallest gap from any region of the culture to any foreign one.
    std::vector<std::pair<int64_t, int>> gaps;
    for (const auto& kv : by_culture)
    {
        int64_t g = INT64_MAX;
        for (const int a : kv.second)
            for (std::size_t b = 0; b < ss.regions.size(); ++b)
            {
                const int fc = ss.regions[b].founding_culture;
                if (fc < 0 || fc == kv.first || ss.regions[b].domain != region_domain::land) continue;
                g = std::min(g, dist2(ss.regions[static_cast<std::size_t>(a)], ss.regions[b], gw));
            }
        gaps.push_back({g, kv.first});
    }
    std::sort(gaps.begin(), gaps.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second; });
    const int c = gaps[0].second;
    r.lc_culture    = c;
    r.lc_gap        = static_cast<int>(gaps[0].first);
    r.lc_second_gap = static_cast<int>(gaps[1].first);
    r.lc_regions    = static_cast<int>(by_culture[c].size());
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
        r.lc_polities = static_cast<int>(cn);
        if (cn > 0 && wn > 0 && ws > 0) r.lc_lag_x100 = static_cast<int>(cs * 100 * wn / (cn * ws));
    }
    // Colonised: a polity of the culture bound or bought by a polity of another.
    std::set<int> cultures_bound;
    for (const lapse_event& e : exp.events)
    {
        if (!(is(e, K::subject_bound) || is(e, K::province_bought))) continue;
        const auto nat = pc.find(e.polity), ovr = pc.find(e.other);
        const int nc = nat == pc.end() ? -1 : nat->second, oc = ovr == pc.end() ? -2 : ovr->second;
        if (nc >= 0 && nc != oc) cultures_bound.insert(nc);
        if (nc == c && oc != c) ++r.lc_bound;
    }
    r.lc_cultures_bound = static_cast<int>(cultures_bound.size());
    r.lc_hit = r.lc_bound > 0 && r.lc_lag_x100 >= 0 && r.lc_lag_x100 <= lc_lag_x100;
}

void read_world_war(const era_timelapse& t, row& r)
{
    if (t.steps.empty()) return;
    // Total population per step, and the span's deepest peak-to-trough drop.
    int64_t peak_pop = 0;
    for (const timelapse_step& st : t.steps)
    {
        int64_t pop = 0;
        for (int i = 0; i < st.sample_count; ++i) pop += t.samples[static_cast<std::size_t>(st.first_sample + i)].population;
        peak_pop = std::max(peak_pop, pop);
        if (peak_pop > 0)
            r.ww_pop_drop_pm = std::max(r.ww_pop_drop_pm, static_cast<int>((peak_pop - pop) * 1000 / peak_pop));
    }
    // Sliding windows: the six most populous at the window's opening step,
    // and how many of them a conflict moment touched inside it.
    for (const timelapse_step& st : t.steps)
    {
        std::vector<std::pair<int64_t, int>> pops;
        for (int i = 0; i < st.sample_count; ++i)
        {
            const polity_sample& s = t.samples[static_cast<std::size_t>(st.first_sample + i)];
            pops.push_back({s.population, s.polity});
        }
        std::sort(pops.begin(), pops.end(), [](const auto& a, const auto& b) {
            return a.first != b.first ? a.first > b.first : a.second < b.second; });
        std::set<int> top;
        for (std::size_t i = 0; i < pops.size() && i < static_cast<std::size_t>(ww_top_k); ++i) top.insert(pops[i].second);
        std::set<int> touched;
        int moments = 0;
        for (const lapse_event& e : t.events)
        {
            if (e.year < st.year || e.year >= st.year + ww_window_y) continue;
            if (!(is(e, K::seat_captured) || is(e, K::realm_ended) || is(e, K::sea_leg_campaign) || is(e, K::treaty_broken)))
                continue;
            ++moments;
            if (top.count(e.polity)) touched.insert(e.polity);
            if (top.count(e.other))  touched.insert(e.other);
        }
        if (static_cast<int>(touched.size()) > r.ww_belligerents)
        {
            r.ww_belligerents = static_cast<int>(touched.size());
            r.ww_window_start = st.year;
            r.ww_moments      = moments;
        }
    }
    r.ww_hit = r.ww_belligerents >= ww_min_belligerents;
}

/// A split followed by its two halves fighting: returns (fights, splits).
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

constexpr int cw_min_half_pm = 250; ///< successor >= a quarter of the parent's regions after the split
constexpr int cw_min_regions = 3;

std::pair<int, int> read_split_wars(const era_timelapse& t, bool must_take_parent)
{
    int fights = 0, splits = 0;
    // The parent's capital at the split, for the revolution reading.
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
            if (child < cw_min_regions || parent <= 0 || child * 1000 < parent * cw_min_half_pm) continue;
        }
        ++splits;
        bool fought = false;
        for (std::size_t j = i + 1; j < t.events.size() && !fought; ++j)
        {
            const lapse_event& e = t.events[j];
            if (e.year > s.year + cw_window_y) break;
            const bool child_wins  = e.polity == s.polity && e.other == s.other;
            const bool parent_wins = e.polity == s.other && e.other == s.polity;
            if (must_take_parent)
                fought = child_wins && is(e, K::realm_ended);
            else
                fought = (child_wins || parent_wins) && (is(e, K::seat_captured) || is(e, K::realm_ended));
            // realm_ended carries the DEAD as polity and the killer as other.
            if (is(e, K::realm_ended))
            {
                const bool parent_dies = e.polity == s.other && e.other == s.polity;
                const bool child_dies  = e.polity == s.polity && e.other == s.other;
                fought = must_take_parent ? parent_dies : (parent_dies || child_dies);
            }
            if (must_take_parent && is(e, K::seat_captured) && e.polity == s.polity && e.other == s.other)
                fought = true; // the successor took a seat of its parent's: read as taking the nation's centre
        }
        if (fought) ++fights;
    }
    return {fights, splits};
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    const char* csv = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            std::stringstream ss(argv[++i]);
            std::string tok;
            while (std::getline(ss, tok, ',')) if (!tok.empty()) seeds.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
        }
        else if (std::strcmp(argv[i], "--csv") == 0 && i + 1 < argc) csv = argv[++i];
    }
    if (seeds.empty()) seeds = library_seeds("docs/generation/seed_library.json");
    if (seeds.empty()) { std::printf("FATAL  no seeds (docs/generation/seed_library.json)\n"); return 1; }

    // The shipped data layer, in app order (app::ensure_works_loaded): a build
    // without the works table runs Industrialisation with no scale credit.
    lua_state        lua;
    world_gen_config cfg{};
    works_registry   works;
    lua.load("scripts/world_gen.lua");
    cfg.load_from_lua(lua);
    lua.load("scripts/works.lua");
    works.load_from_lua(lua);

    std::vector<row> rows;
    for (const uint32_t seed : seeds)
    {
        world_params p = arc_params(world_arc::shipped);
        p.seed = seed;
        generation_report     rep{};
        era_minus_one_fixture fx{};
        const auto t0 = std::chrono::steady_clock::now();
        world w = make_hard_coded_world(p, &rep, cfg, nullptr, &works, &fx);
        row r;
        r.seed    = seed;
        r.build_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

        const generation_report::body_entry* home = nullptr;
        for (const generation_report::body_entry& b : rep.bodies) if (b.is_homeworld) { home = &b; break; }
        if (home == nullptr) { std::printf("seed %u: no homeworld entry\n", seed); continue; }

        const era_timelapse& emp = home->prehistory_timelapse;
        const era_timelapse& exp = home->exploration_timelapse;
        const era_timelapse& ind = home->industrialisation_timelapse;
        std::printf("seed %u records: empires %zu steps %zu events, exploration %zu/%zu, industrialisation %zu/%zu\n",
                    seed, emp.steps.size(), emp.events.size(), exp.steps.size(), exp.events.size(),
                    ind.steps.size(), ind.events.size());
        const std::map<int, int> pc = polity_cultures({&emp, &exp, &ind}, home->settlement);

        read_rise_and_fall(emp, r);
        read_late_culture(emp, exp, home->settlement, fx.gw, pc, r);
        read_world_war(ind, r);
        const auto cw = read_split_wars(emp, false);
        const auto rv = read_split_wars(ind, true);
        r.cw_count = cw.first; r.cw_breakaways = cw.second;
        r.rv_count = rv.first; r.rv_splits     = rv.second;

        // BL-1278's measure-first.
        // A budget world charters its specialists in finish_campaign_world,
        // after this build; the carve ledger carries the PLAN it will spend
        // (plan_charters_by_nation). A roster world has them on the world now.
        std::set<entity_id> with_spec;
        for (const auto& kv : w.nations) { (void)kv; ++r.nations; }
        if (rep.carve_competitors_from_budget)
        {
            for (const auto& c : rep.carve_competitors)
                if (c.planned_specialists > 0) { r.specialists += static_cast<int>(c.planned_specialists); with_spec.insert(c.nation); }
        }
        else
            for (const auto& kv : w.corporations)
                if (!kv.second.is_background && kv.second.home_nation != null_entity)
                { ++r.specialists; with_spec.insert(kv.second.home_nation); }
        r.nations_with_specialist = static_cast<int>(with_spec.size());
        const era_timelapse& last = !ind.steps.empty() ? ind : (!exp.steps.empty() ? exp : emp);
        if (!last.steps.empty()) r.living_polities = last.steps.back().sample_count;

        std::printf("seed %-3u (%.0f s)  RF %s realm %d %d‰ at -200, peak %d‰ in %d x%.2f, %d seats, fall %dy to %d‰, %d split%s, levels %d/%d  |"
                    "  LC %s culture %d gap %d (next %d) %d realm%s lag %d%% bound %d (%d/%d cultures colonised)  |"
                    "  WW %s %d/6 in %d-%d, pop drop %d‰  |  CW %d/%d  RV %d/%d  |  nations %d, %d w/ specialist, polities %d\n",
                    seed, r.build_s, r.rf_hit ? "HIT " : "miss", r.rf_polity, r.rf_start_q, r.rf_peak_q, r.rf_peak_year,
                    r.rf_margin_x100 / 100.0, r.rf_captures, r.rf_fall_years, r.rf_fall_q, r.rf_breakaways,
                    r.rf_ended ? " ended" : "", r.rf_rise_level, r.rf_fall_level,
                    r.lc_hit ? "HIT " : "miss", r.lc_culture, r.lc_gap, r.lc_second_gap, r.lc_polities, r.lc_polities == 1 ? "" : "s", r.lc_lag_x100, r.lc_bound,
                    r.lc_cultures_bound, r.lc_cultures,
                    r.ww_hit ? "HIT " : "miss", r.ww_belligerents, r.ww_window_start, r.ww_window_start + ww_window_y,
                    r.ww_pop_drop_pm, r.cw_count, r.cw_breakaways, r.rv_count, r.rv_splits,
                    r.nations, r.nations_with_specialist, r.living_polities);
        if (rep.handoff_invalid)
            std::printf("seed %u: HANDOFF VIOLATION %s\n", seed, rep.handoff_violation.c_str());
        std::fflush(stdout);
        rows.push_back(r);
    }

    int rf = 0, lc = 0, ww = 0, cw = 0, rv = 0, hegemon = 0;
    for (const row& r : rows)
    {
        rf += r.rf_hit; lc += r.lc_hit; ww += r.ww_hit; cw += r.cw_count > 0; rv += r.rv_count > 0;
        hegemon += r.rf_peak_q >= hegemony_q;
    }
    const int n = static_cast<int>(rows.size());
    std::printf("\nhistory_patterns over %d seeds (RF ruled: floor %d‰, peak by %d, halves within %d y; the rest provisional):\n",
                n, rise_fall_floor_pm, rise_fall_peak_by_year, rise_fall_window_years);
    std::printf("  rise and fall  %2d/%d   (targets: always)   %d seeds peaked at hegemony\n", rf, n, hegemon);
    std::printf("  late culture   %2d/%d   (targets: always)\n", lc, n);
    std::printf("  world war      %2d/%d   (targets: always)\n", ww, n);
    std::printf("  civil war      %2d/%d   (lean)\n", cw, n);
    std::printf("  revolution     %2d/%d   (lean)\n", rv, n);

    if (csv != nullptr)
    {
        std::ofstream o(csv);
        o << "seed,rf_hit,rf_polity,rf_start_q,rf_peak_q,rf_peak_year,rf_margin_x100,rf_peak_frac_pm,rf_captures,"
             "rf_fall_years,rf_fall_q,rf_breakaways,rf_ended,"
             "lc_hit,lc_culture,lc_gap,lc_second_gap,lc_regions,lc_polities,lc_lag_x100,lc_bound,lc_cultures_bound,lc_cultures,"
             "ww_hit,ww_belligerents,ww_window_start,ww_moments,ww_pop_drop_pm,cw_count,cw_breakaways,rv_count,rv_splits,"
             "nations,nations_with_specialist,specialists,living_polities\n";
        for (const row& r : rows)
            o << r.seed << ',' << r.rf_hit << ',' << r.rf_polity << ',' << r.rf_start_q << ',' << r.rf_peak_q << ','
              << r.rf_peak_year << ',' << r.rf_margin_x100 << ',' << r.rf_peak_frac_pm << ','
              << r.rf_captures << ',' << r.rf_fall_years << ',' << r.rf_fall_q << ',' << r.rf_breakaways << ',' << r.rf_ended << ','
              << r.lc_hit << ',' << r.lc_culture << ',' << r.lc_gap << ',' << r.lc_second_gap << ',' << r.lc_regions << ','
              << r.lc_polities << ',' << r.lc_lag_x100 << ',' << r.lc_bound << ',' << r.lc_cultures_bound << ',' << r.lc_cultures << ','
              << r.ww_hit << ',' << r.ww_belligerents << ',' << r.ww_window_start << ',' << r.ww_moments << ','
              << r.ww_pop_drop_pm << ',' << r.cw_count << ',' << r.cw_breakaways << ',' << r.rv_count << ','
              << r.rv_splits << ',' << r.nations << ',' << r.nations_with_specialist << ',' << r.specialists << ','
              << r.living_polities << '\n';
    }
    return 0;
}
