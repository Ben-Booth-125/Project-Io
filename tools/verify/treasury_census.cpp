// ---------------------------------------------------------------------------
// treasury_census — where the history's chests go (BL-1298, MEASURE FIRST)
// ---------------------------------------------------------------------------
// NATION_GENERATION.md § Pass 7: every nation opens with real credits and none
// in debt; the lever is "the history banks more". Before any design moves, this
// census reads, per seed and pooled over the seed library:
//
//   1. OPENING TREASURIES (nation_component::treasury, credits) on the
//      homeworld, twice: straight after make_hard_coded_world (the Pass 7 fold
//      of the last span's chests) and after finish_campaign_world (the search's
//      state charters and the twelve-quarter settle). Count, sum, min, p10,
//      median, p90, max, how many under 1 and under 10 credits; the same split
//      MAJORS (a nation holding a non-background specialist after the finish)
//      versus minors.
//   2. THE LEVY: each nation's extraction levy (NATIONS.md § 3) in settle
//      quarter one and quarter twelve, read off the corporations' quarterly
//      returns (`quarterly_return::levies`) and ATTRIBUTED TO THE PAYER'S
//      home_nation -- an approximation where a firm extracts across a border;
//      the world total is exact. The opening is then expressed in quarters of
//      levy.
//   3. WHERE THE CHEST WENT, per span (Empires, Exploration, Industrialisation),
//      pooled over polities: earned by source (endowment, corridor, trade, the
//      1200 consolidation of seat stores), moved between polities (tribute,
//      purchases), spent by sink (army bill, navy bill, port/navy/army builds,
//      post roads, the treasury -> industry points debit), the stock at the
//      span's open and close, the chest left on ground no polity holds, and the
//      residual the flows do not explain. The earn and consolidation counters
//      are BL-1298's write-only report fields on history_sim_state.
//   4. CONCENTRATION: richest / median chest, and the top-3 nations' share.
//
// Exits 0 always: a census, not a gate. Release build:
//
//   bash tools/verify/build_lua_harness.sh treasury_census
//   build_gen/verify/treasury_census.exe [--seeds 0,11] [--no-finish]

#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/era_minus_one.hpp"
#include "world/finish_campaign_world.hpp"
#include "world/hard_coded_world.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"
#include "world/works_roster.hpp"
#include "world/world_gen_config.hpp"
#include "scripting/lua_state.hpp"

#include <algorithm>
#include <cmath>
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

// BL-1298 calibration knobs (-1 = the shipped world_params default) and the
// optional per-nation dump (seed, name, build credits, finish credits, major).
int   g_levy  = -1;
int   g_endow = -1;
long long g_ipu = -1;   // --ipu N: industry points per treasury unit (-1 = shipped)
bool  g_no_size = false; // --no-size: the army is NOT sized to the earn (the pre-ruling arm)
FILE* g_dump  = nullptr;

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

double pct(std::vector<double> v, int pm)
{
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t i = std::min(v.size() - 1, (v.size() * static_cast<std::size_t>(pm)) / 1000);
    return v[i];
}

double spearman_rho(const std::vector<std::pair<double, double>>& xy)
{
    const std::size_t n = xy.size();
    if (n < 3) return 0.0;
    auto ranks = [n](std::vector<double> v) {
        std::vector<std::size_t> ord(n);
        for (std::size_t i = 0; i < n; ++i) ord[i] = i;
        std::sort(ord.begin(), ord.end(), [&](std::size_t a, std::size_t b) { return v[a] < v[b] || (v[a] == v[b] && a < b); });
        std::vector<double> r(n);
        for (std::size_t i = 0; i < n;)
        {
            std::size_t j = i;
            while (j + 1 < n && v[ord[j + 1]] == v[ord[i]]) ++j;
            for (std::size_t k = i; k <= j; ++k) r[ord[k]] = 0.5 * static_cast<double>(i + j);
            i = j + 1;
        }
        return r;
    };
    std::vector<double> a(n), b(n);
    for (std::size_t i = 0; i < n; ++i) { a[i] = xy[i].first; b[i] = xy[i].second; }
    const std::vector<double> ra = ranks(a), rb = ranks(b);
    double ma = 0, mb = 0;
    for (std::size_t i = 0; i < n; ++i) { ma += ra[i]; mb += rb[i]; }
    ma /= static_cast<double>(n); mb /= static_cast<double>(n);
    double cov = 0, va = 0, vb = 0;
    for (std::size_t i = 0; i < n; ++i)
    {
        cov += (ra[i] - ma) * (rb[i] - mb); va += (ra[i] - ma) * (ra[i] - ma); vb += (rb[i] - mb) * (rb[i] - mb);
    }
    return (va > 0 && vb > 0) ? cov / std::sqrt(va * vb) : 0.0;
}

/// How many of the three largest 1200 folds are among the three richest chests at the close.
int top3_overlap(std::vector<std::pair<double, double>> xy)
{
    const std::size_t n = xy.size();
    std::vector<std::size_t> by_fold(n), by_close(n);
    for (std::size_t i = 0; i < n; ++i) by_fold[i] = by_close[i] = i;
    std::sort(by_fold.begin(), by_fold.end(), [&](std::size_t a, std::size_t b) { return xy[a].first > xy[b].first || (xy[a].first == xy[b].first && a < b); });
    std::sort(by_close.begin(), by_close.end(), [&](std::size_t a, std::size_t b) { return xy[a].second > xy[b].second || (xy[a].second == xy[b].second && a < b); });
    int k = 0;
    for (std::size_t i = 0; i < std::min<std::size_t>(3, n); ++i)
        for (std::size_t j = 0; j < std::min<std::size_t>(3, n); ++j)
            if (by_fold[i] == by_close[j]) ++k;
    return k;
}

double median(std::vector<double> v)
{
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    return (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

void print_dist(const char* label, const std::vector<double>& v)
{
    if (v.empty()) { std::printf("    %-22s n=0\n", label); return; }
    double sum = 0, mn = v[0], mx = v[0];
    int u1 = 0, u10 = 0;
    for (double x : v) { sum += x; mn = std::min(mn, x); mx = std::max(mx, x); u1 += x < 1.0; u10 += x < 10.0; }
    std::printf("    %-22s n=%-4zu sum=%11.1f min=%8.2f p10=%8.2f med=%8.2f p90=%9.1f max=%9.1f  <1:%-3d <10:%-3d\n",
                label, v.size(), sum, mn, pct(v, 100), median(v), pct(v, 900), mx, u1, u10);
}

void print_conc(const char* label, std::vector<double> v)
{
    if (v.empty()) return;
    std::sort(v.begin(), v.end(), [](double a, double b) { return a > b; });
    double sum = 0;
    for (double x : v) sum += x;
    const double top3 = v[0] + (v.size() > 1 ? v[1] : 0) + (v.size() > 2 ? v[2] : 0);
    const double med  = median(v);
    std::printf("    %-22s richest/median=%s  top-3 share=%.1f%%\n", label,
                med > 0 ? (std::to_string(static_cast<long long>(v[0] / med + 0.5)) + "x").c_str() : "inf (median 0)",
                sum > 0 ? 100.0 * top3 / sum : 0.0);
}

/// One span's chest ledger, in sim material units (1 credit = 100,000).
struct span_flows
{
    bool    ran = false;
    int64_t open_stock = 0, close_stock = 0, close_orphan = 0;
    int64_t earn_endow = 0, earn_corr = 0, earn_trade = 0, consolidation = 0;
    int64_t tribute = 0, purchases = 0;
    int64_t army_bill = 0, navy_bill = 0, ports = 0, navies = 0, armies = 0, roads = 0, industry = 0;
    int64_t war = 0; ///< BL-1289: the world war's stated cost
    int64_t kept_by_crowding = 0;
    int64_t levy = 0, disbanded = 0, sized_rounds = 0, ip_from_treasury = 0, army_steps = 0;
    void add(const span_flows& o)
    {
        ran = ran || o.ran;
        open_stock += o.open_stock; close_stock += o.close_stock; close_orphan += o.close_orphan;
        earn_endow += o.earn_endow; earn_corr += o.earn_corr; earn_trade += o.earn_trade;
        consolidation += o.consolidation; tribute += o.tribute; purchases += o.purchases;
        army_bill += o.army_bill; navy_bill += o.navy_bill; ports += o.ports; navies += o.navies;
        armies += o.armies; roads += o.roads; industry += o.industry; war += o.war; kept_by_crowding += o.kept_by_crowding;
        levy += o.levy; disbanded += o.disbanded; sized_rounds += o.sized_rounds; ip_from_treasury += o.ip_from_treasury; army_steps += o.army_steps;
    }
    int64_t earned() const { return earn_endow + earn_corr + earn_trade + consolidation + levy; }
    int64_t spent() const { return army_bill + navy_bill + ports + navies + armies + roads + industry + war; }
};

span_flows read_span(const history_sim_state& st, const std::vector<region>& open,
                     const std::vector<region>& close)
{
    span_flows f;
    f.ran = true;
    for (const region& r : open) f.open_stock += r.treasury;
    for (const region& r : close)
    {
        f.close_stock += r.treasury;
        if (r.nation < 0) f.close_orphan += r.treasury;
    }
    f.earn_endow    = st.treasury_earned_endowment;
    f.earn_corr     = st.treasury_earned_corridor;
    f.earn_trade    = st.treasury_earned_trade;
    f.consolidation = st.treasury_from_consolidation;
    f.tribute       = st.tribute_remitted;
    f.purchases     = st.treasury_spent_on_purchases;
    f.army_bill     = st.treasury_spent_on_army_upkeep;
    f.navy_bill     = st.treasury_spent_on_navy_upkeep;
    f.ports         = st.treasury_spent_on_ports;
    f.navies        = st.treasury_spent_on_navies;
    f.armies        = st.treasury_spent_on_standing_armies;
    f.roads         = st.treasury_spent_on_roads;
    f.industry      = st.treasury_spent_on_industry;
    f.war           = st.world_war_treasury_spent; // BL-1289
    f.kept_by_crowding = st.treasury_kept_by_crowding;
    f.levy             = st.treasury_earned_levy;       // BL-1298
    f.disbanded        = st.army_disbanded_by_earn;
    f.sized_rounds     = st.army_sized_rounds;
    f.army_steps       = st.army_steps_bought;
    f.ip_from_treasury = st.industry_points_from_treasury;
    return f;
}

constexpr double kCr = 1.0e-5; // Pass 7's 0.01 per mille: credits per material unit

void print_span(const char* name, const span_flows& f)
{
    if (!f.ran) { std::printf("    %-17s did not run\n", name); return; }
    const double e = static_cast<double>(std::max<int64_t>(f.earned(), 1));
    auto sh = [&](int64_t x) { return 100.0 * static_cast<double>(x) / e; };
    const int64_t resid = f.open_stock + f.earned() - f.spent() - f.close_stock;
    std::printf("    %-17s open %10.0f cr  earned %10.0f cr  spent %10.0f cr  close %10.0f cr (on unheld ground %.0f)  residual %.0f cr\n",
                name, f.open_stock * kCr, f.earned() * kCr, f.spent() * kCr, f.close_stock * kCr,
                f.close_orphan * kCr, resid * kCr);
    std::printf("      earn  endowment %9.0f (%4.1f%%)  corridor %9.0f (%4.1f%%)  trade %9.0f (%4.1f%%)  consolidation %9.0f (%4.1f%%)\n",
                f.earn_endow * kCr, sh(f.earn_endow), f.earn_corr * kCr, sh(f.earn_corr),
                f.earn_trade * kCr, sh(f.earn_trade), f.consolidation * kCr, sh(f.consolidation));
    std::printf("      levy  %9.0f (%4.1f%%)  | army sized to earn: %lld polity-rounds, %lld heads disbanded; army steps bought %lld | industry points from treasury %lld\n",
                f.levy * kCr, sh(f.levy), static_cast<long long>(f.sized_rounds),
                static_cast<long long>(f.disbanded), static_cast<long long>(f.army_steps),
                static_cast<long long>(f.ip_from_treasury));
    std::printf("      spend army bill %9.0f (%4.1f%%)  navy bill %9.0f (%4.1f%%)  ports %8.0f (%4.1f%%)  navies %8.0f (%4.1f%%)  armies %8.0f (%4.1f%%)\n",
                f.army_bill * kCr, sh(f.army_bill), f.navy_bill * kCr, sh(f.navy_bill),
                f.ports * kCr, sh(f.ports), f.navies * kCr, sh(f.navies), f.armies * kCr, sh(f.armies));
    std::printf("            post roads %8.0f (%4.1f%%)  treasury->industry %9.0f (%4.1f%%)  world war %9.0f (%4.1f%%)  [kept by crowding %.0f]\n",
                f.roads * kCr, sh(f.roads), f.industry * kCr, sh(f.industry), f.war * kCr, sh(f.war),
                f.kept_by_crowding * kCr);
    std::printf("      moved tribute %9.0f (%4.1f%%)  purchases %9.0f (%4.1f%%)   (polity -> polity; zero-sum)\n",
                f.tribute * kCr, sh(f.tribute), f.purchases * kCr, sh(f.purchases));
}

struct seed_out
{
    uint32_t seed = 0;
    std::vector<double> t0, t1, t0_major, t0_minor, t1_major, t1_minor;
    std::vector<double> levy_q1, levy_q12;   ///< per homeworld nation, home_nation attribution
    std::vector<double> quarters_t0;         ///< t0 / levy_q1 where levy > 0
    std::vector<double> quarters_t1;
    double levy_q1_world = 0, levy_q12_world = 0, levy_q1_unattributed = 0;
    int    off_home_nations = 0;
    double polity_chest_credits = 0; ///< setup_polity_treasuries summed, in credits
    int    polities_with_chest = 0;
    /// Polities alive at the last close, split by whether they stood at 1200
    /// (and so took the consolidation) or rose after it: their chests, credits.
    std::vector<double> chest_old, chest_new;
    /// Per living polity at the close: the 1200 consolidation it took (its
    /// held seats' material stock at the span's open, credits), and its fate.
    std::vector<double> fold_1200;
    /// Per living polity at the close, (its 1200 fold, its chest), credits:
    /// the in-world reading of "keep the order" -- do the realms that
    /// inherited empires' chests at 1200 still hold the richest at the close.
    std::vector<std::pair<double, double>> fold_vs_close;
    std::vector<double> order_rho; ///< per seed (pooled block only)
    int top3_kept = 0, top3_seeds = 0;
    /// Industry points standing at the 1960 close: all of them, and the
    /// treasury-paid tally (`region::industry_points_from_treasury`); the
    /// scale term is the difference. Per-seed treasury shares (pooled block).
    double ip_total = 0, ip_treasury = 0;
    std::vector<double> ip_share;
    int fate_never = 0;   ///< took < 1 cr at 1200
    int fate_drained = 0; ///< took >= 1 cr at 1200, holds < 1 cr at the close
    int fate_kept = 0;    ///< holds >= 1 cr at the close
    double drained_cr = 0; ///< what the drained ones took at 1200
    double charter_paid = 0;
    span_flows emp, exp, ind;
    int64_t empires_material_at_1200 = 0;
    double build_s = 0, finish_s = 0;
};

struct data_layer
{
    lua_state        lua;
    world_gen_config cfg{};
    works_registry   works;
    recipe_registry  reg;
};

seed_out run_seed(const data_layer& dl, uint32_t seed, bool do_finish)
{
    seed_out o;
    o.seed = seed;
    world_params p = arc_params(world_arc::shipped);
    p.seed = seed;
    if (g_levy  >= 0) p.treasury_levy_per_mille_q   = g_levy;
    if (g_endow >= 0) p.treasury_endowment_income_q = g_endow;
    if (g_no_size)    p.standing_army_sized_to_earn = false;
    if (g_ipu >= 1)   p.industry_points_per_1000_treasury_units = g_ipu;
    generation_report     rep{};
    era_minus_one_fixture fx{};
    const auto t0 = std::chrono::steady_clock::now();
    world w = make_hard_coded_world(p, &rep, dl.cfg, nullptr, &dl.works, &fx);
    const auto t1 = std::chrono::steady_clock::now();
    o.build_s = std::chrono::duration<double>(t1 - t0).count();

    const generation_report::body_entry* home = nullptr;
    for (const generation_report::body_entry& b : rep.bodies)
        if (b.is_homeworld) { home = &b; break; }
    const entity_id home_body = home ? home->id : null_entity;

    // Homeworld nations, in id order.
    std::vector<entity_id> ids;
    for (const auto& kv : w.nations)
    {
        const nation_component& n = kv.second;
        bool on_home = false;
        if (!n.tiles.empty())
        {
            const auto t = w.tiles.find(n.tiles.front());
            on_home = t != w.tiles.end() && t->second.body == home_body;
        }
        if (on_home) ids.push_back(kv.first); else ++o.off_home_nations;
    }
    std::sort(ids.begin(), ids.end());
    std::map<entity_id, double> chest0;
    for (const entity_id id : ids) chest0[id] = w.nations.at(id).treasury;

    for (const int64_t t : fx.setup_polity_treasuries)
        if (t > 0) { o.polity_chest_credits += static_cast<double>(t) * kCr; ++o.polities_with_chest; }

    // Old polities (alive at 1200, consolidated) vs new (risen since).
    {
        std::set<int> at_1200;
        for (const polity& q : fx.pre_exploration_polities) if (q.alive) at_1200.insert(q.id);
        const std::vector<polity>& last = fx.industrialisation_ran ? fx.industrialisation_handoff.polities
                                                                   : fx.exploration_handoff.polities;
        for (const polity& q : last)
        {
            if (!q.alive) continue;
            const double c = (q.id >= 0 && static_cast<std::size_t>(q.id) < fx.setup_polity_treasuries.size())
                ? static_cast<double>(fx.setup_polity_treasuries[static_cast<std::size_t>(q.id)]) * kCr : 0.0;
            (at_1200.count(q.id) ? o.chest_old : o.chest_new).push_back(c);
            // The 1200 fold, re-read off the captured span-open regions the
            // way the consolidation reads it: every held seat, plus the capital.
            int64_t folded = 0;
            const auto& rg = fx.pre_exploration_settlement.regions;
            int cap = -1;
            for (const polity& p0 : fx.pre_exploration_polities) if (p0.id == q.id) { cap = p0.alive ? p0.capital : -1; break; }
            for (std::size_t i = 0; i < rg.size(); ++i)
                if (rg[i].nation == q.id && (rg[i].is_seat || static_cast<int>(i) == cap)) folded += rg[i].material_stock;
            const double f = static_cast<double>(folded) * kCr;
            o.fold_1200.push_back(f);
            o.fold_vs_close.emplace_back(f, c);
            if (f < 1.0) ++o.fate_never;
            else if (c < 1.0) { ++o.fate_drained; o.drained_cr += f; }
            else ++o.fate_kept;
        }
        int alive_last = 0;
        for (const polity& q : last) alive_last += q.alive ? 1 : 0;
        std::printf("  [polities: %zu in the 1200 table (%zu alive), %zu in the last close's (%d alive)]\n",
                    fx.pre_exploration_polities.size(), at_1200.size(), last.size(), alive_last);
    }

    // Spans. Empires runs no treasury (exploration_upkeep_enabled is false
    // throughout it): its chest is the seats' material stock, folded at 1200.
    o.emp.ran = fx.ran;
    for (const region& r : fx.pre_exploration_settlement.regions) o.empires_material_at_1200 += r.material_stock;
    if (fx.exploration_ran)
        o.exp = read_span(fx.exploration_state, fx.pre_exploration_settlement.regions, fx.exploration_handoff.regions);
    if (fx.industrialisation_ran)
        for (const region& r : fx.industrialisation_handoff.regions)
        {
            o.ip_total    += static_cast<double>(r.industry_points);
            o.ip_treasury += static_cast<double>(r.industry_points_from_treasury);
        }
    if (fx.industrialisation_ran)
        o.ind = read_span(fx.industrialisation_state, fx.industrialisation_open_regions,
                          fx.industrialisation_handoff.regions);

    std::set<entity_id> majors;
    if (do_finish)
    {
        recipe_registry seed_reg = dl.reg; // the worker's copy, banded inside the finish
        const finish_campaign_result fr = finish_campaign_world(w, rep, seed_reg, p, dl.cfg, nullptr);
        (void)fr;
        o.finish_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count();
        for (const auto& kv : w.corporations)
            if (!kv.second.is_background && kv.second.home_nation != null_entity)
                majors.insert(kv.second.home_nation);

        // The levy, settle quarters 1 and 12, attributed to the payer's home.
        std::map<entity_id, double> q1, q12;
        for (const auto& kv : w.corporations)
        {
            const corporation_component& c = kv.second;
            const std::size_t n = c.returns.size();
            const auto at = [&](int k) -> double {
                const long idx = static_cast<long>(n) - (k_campaign_settle_ticks - k);
                return (idx >= 0 && static_cast<std::size_t>(idx) < n) ? c.returns[static_cast<std::size_t>(idx)].levies : 0.0;
            };
            const double a = at(0), b = at(k_campaign_settle_ticks - 1);
            o.levy_q1_world += a; o.levy_q12_world += b;
            if (chest0.count(c.home_nation)) { q1[c.home_nation] += a; q12[c.home_nation] += b; }
            else o.levy_q1_unattributed += a;
        }
        for (const entity_id id : ids)
        {
            o.levy_q1.push_back(q1[id]);
            o.levy_q12.push_back(q12[id]);
        }
    }

    for (const entity_id id : ids)
    {
        const double a = chest0[id];
        const bool maj = majors.count(id) > 0;
        o.t0.push_back(a);
        (maj ? o.t0_major : o.t0_minor).push_back(a);
        if (do_finish)
        {
            const double b = w.nations.at(id).treasury;
            o.t1.push_back(b);
            (maj ? o.t1_major : o.t1_minor).push_back(b);
        }
        if (g_dump)
            std::fprintf(g_dump, "%u,%s,%.4f,%.4f,%d\n", seed, w.nations.at(id).name.c_str(), a,
                         do_finish ? w.nations.at(id).treasury : 0.0, maj ? 1 : 0);
    }
    if (do_finish)
        for (std::size_t i = 0; i < ids.size(); ++i)
            if (o.levy_q1[i] > 0)
            {
                o.quarters_t0.push_back(o.t0[i] / o.levy_q1[i]);
                o.quarters_t1.push_back(o.t1[i] / o.levy_q1[i]);
            }
    return o;
}

void print_seed_block(const seed_out& o, bool do_finish)
{
    std::printf("  OPENING TREASURIES, credits (homeworld nations; %d off-homeworld nation%s not counted)\n",
                o.off_home_nations, o.off_home_nations == 1 ? "" : "s");
    print_dist("after build (Pass 7)", o.t0);
    print_dist("  majors", o.t0_major);
    print_dist("  minors", o.t0_minor);
    if (do_finish)
    {
        print_dist("after finish", o.t1);
        print_dist("  majors", o.t1_major);
        print_dist("  minors", o.t1_minor);
        std::printf("  LEVY (home_nation attribution): world q1 %.1f cr, q12 %.1f cr, q1 unattributed %.1f\n",
                    o.levy_q1_world, o.levy_q12_world, o.levy_q1_unattributed);
        print_dist("levy q1 per nation", o.levy_q1);
        print_dist("levy q12 per nation", o.levy_q12);
        print_dist("build chest / q1 levy", o.quarters_t0);
        print_dist("finish chest / q1 levy", o.quarters_t1);
    }
    std::printf("  POLITY CHESTS at the last close, credits (living polities)\n");
    print_dist("stood at 1200", o.chest_old);
    print_dist("rose after 1200", o.chest_new);
    print_dist("their 1200 fold", o.fold_1200);
    if (o.order_rho.empty())
        std::printf("    ORDER  rank(1200 fold) vs rank(close chest): spearman %.3f, top-3 folds among top-3 chests %d/3\n",
                    spearman_rho(o.fold_vs_close), top3_overlap(o.fold_vs_close));
    else
        std::printf("    ORDER  per-seed spearman(1200 fold, close chest) median %.3f; top-3 folds among top-3 chests %d/%d\n",
                    median(o.order_rho), o.top3_kept, 3 * o.top3_seeds);
    std::printf("    fate: took <1 cr at 1200 %d | took >=1 cr, <1 cr at close %d (they took %.0f cr) | >=1 cr at close %d\n",
                o.fate_never, o.fate_drained, o.drained_cr, o.fate_kept);
    if (o.ip_total > 0)
        std::printf("  INDUSTRY POINTS at the close: %.4g total = scale %.4g + treasury %.4g (treasury share %.3f%s)\n",
                    o.ip_total, o.ip_total - o.ip_treasury, o.ip_treasury, o.ip_treasury / o.ip_total,
                    o.ip_share.empty() ? "" : (", per-seed median " + std::to_string(median(o.ip_share))).c_str());
    std::printf("  CONCENTRATION\n");
    print_conc("after build", o.t0);
    if (do_finish) print_conc("after finish", o.t1);
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    bool do_finish = true;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--levy") == 0 && i + 1 < argc) { g_levy = std::atoi(argv[++i]); continue; }
        if (std::strcmp(argv[i], "--ipu") == 0 && i + 1 < argc) { g_ipu = std::atoll(argv[++i]); continue; }
        if (std::strcmp(argv[i], "--no-size") == 0) { g_no_size = true; continue; }
        if (std::strcmp(argv[i], "--endow") == 0 && i + 1 < argc) { g_endow = std::atoi(argv[++i]); continue; }
        if (std::strcmp(argv[i], "--dump") == 0 && i + 1 < argc) { g_dump = std::fopen(argv[++i], "wb"); continue; }
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            std::stringstream ss(argv[++i]);
            std::string tok;
            while (std::getline(ss, tok, ',')) if (!tok.empty()) seeds.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
        }
        else if (std::strcmp(argv[i], "--no-finish") == 0) do_finish = false;
        else { std::printf("usage: treasury_census [--seeds a,b,c] [--no-finish] [--levy N] [--endow N] [--dump file.csv]\n"); return 2; }
    }
    if (seeds.empty()) seeds = library_seeds("docs/generation/seed_library.json");
    if (seeds.empty()) { std::printf("FATAL  no seeds (docs/generation/seed_library.json)\n"); return 2; }

    data_layer dl;
    dl.lua.load("scripts/world_gen.lua");
    dl.cfg.load_from_lua(dl.lua);
    dl.lua.load("scripts/works.lua");
    dl.works.load_from_lua(dl.lua);
    if (do_finish)
    {
        dl.lua.load("scripts/recipes.lua");
        dl.lua.load("scripts/economy.lua");
        dl.reg.load_from_lua(dl.lua);
    }

    std::printf("levy %d per mille, endowment %d, ipu %lld (-1 = shipped default)\n", g_levy, g_endow, g_ipu);
    std::printf("treasury_census: %zu seed(s)%s  (1 credit = 100,000 material, Pass 7's 0.01 per mille)\n",
                seeds.size(), do_finish ? ", with finish_campaign_world" : ", build only");
    seed_out pooled;
    std::vector<double> med0, med1;
    for (const uint32_t seed : seeds)
    {
        const seed_out o = run_seed(dl, seed, do_finish);
        std::printf("\n=== seed %u  (build %.0f s, finish %.0f s)  polity chests at the last close: %d holding %.0f cr\n",
                    seed, o.build_s, o.finish_s, o.polities_with_chest, o.polity_chest_credits);
        print_seed_block(o, do_finish);
        std::printf("  WHERE THE CHEST WENT (sim material shown in credits)\n");
        std::printf("    %-17s no treasury flows (upkeep off); seats' material stock at 1200 = %.0f cr\n",
                    "Empires", o.empires_material_at_1200 * kCr);
        print_span("Exploration", o.exp);
        print_span("Industrialisation", o.ind);
        std::fflush(stdout);

        med0.push_back(median(o.t0));
        if (do_finish) med1.push_back(median(o.t1));
        auto cat = [](std::vector<double>& a, const std::vector<double>& b) { a.insert(a.end(), b.begin(), b.end()); };
        cat(pooled.t0, o.t0); cat(pooled.t1, o.t1);
        cat(pooled.t0_major, o.t0_major); cat(pooled.t0_minor, o.t0_minor);
        cat(pooled.t1_major, o.t1_major); cat(pooled.t1_minor, o.t1_minor);
        cat(pooled.levy_q1, o.levy_q1); cat(pooled.levy_q12, o.levy_q12);
        cat(pooled.chest_old, o.chest_old); cat(pooled.chest_new, o.chest_new);
        cat(pooled.fold_1200, o.fold_1200);
        pooled.order_rho.push_back(spearman_rho(o.fold_vs_close));
        pooled.ip_total += o.ip_total; pooled.ip_treasury += o.ip_treasury;
        if (o.ip_total > 0) pooled.ip_share.push_back(o.ip_treasury / o.ip_total);
        pooled.top3_kept += top3_overlap(o.fold_vs_close);
        ++pooled.top3_seeds;
        pooled.fate_never += o.fate_never; pooled.fate_drained += o.fate_drained;
        pooled.fate_kept += o.fate_kept; pooled.drained_cr += o.drained_cr;
        cat(pooled.quarters_t0, o.quarters_t0); cat(pooled.quarters_t1, o.quarters_t1);
        pooled.levy_q1_world += o.levy_q1_world; pooled.levy_q12_world += o.levy_q12_world;
        pooled.levy_q1_unattributed += o.levy_q1_unattributed;
        pooled.off_home_nations += o.off_home_nations;
        pooled.polity_chest_credits += o.polity_chest_credits;
        pooled.polities_with_chest += o.polities_with_chest;
        pooled.empires_material_at_1200 += o.empires_material_at_1200;
        pooled.exp.add(o.exp); pooled.ind.add(o.ind);
    }

    std::printf("\n=== POOLED over %zu seeds  (polity chests at the last close: %d holding %.0f cr)\n",
                seeds.size(), pooled.polities_with_chest, pooled.polity_chest_credits);
    print_seed_block(pooled, do_finish);
    std::printf("  per-seed medians after build:");
    for (double m : med0) std::printf(" %.2f", m);
    std::printf("\n");
    if (do_finish)
    {
        std::printf("  per-seed medians after finish:");
        for (double m : med1) std::printf(" %.2f", m);
        std::printf("\n");
    }
    std::printf("  WHERE THE CHEST WENT, pooled\n");
    std::printf("    %-17s no treasury flows (upkeep off); seats' material stock at 1200 = %.0f cr\n",
                "Empires", pooled.empires_material_at_1200 * kCr);
    print_span("Exploration", pooled.exp);
    print_span("Industrialisation", pooled.ind);
    if (g_dump) std::fclose(g_dump);
    return 0;
}
