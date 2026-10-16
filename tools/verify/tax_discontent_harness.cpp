// ---------------------------------------------------------------------------
// tax_discontent_harness — BL-1335 (taxes and Discontent), the headless proof
// ---------------------------------------------------------------------------
// docs/politics/NATIONS.md § Taxes and Discontent (Ben, 2026-10-16). On the
// SHIPPED start (harness_params.hpp's app mirror), seed 0 by default:
//
//   T1  THE TAX CONSERVES: one real tick's money-loop budgets, taxed by
//       run_nation_taxes on a copy — every taxed firm's debit is exactly its
//       profit x its home nation's tier rate, landed on its balance and on
//       `budgets[corp].taxes`; every treasury moved by exactly its firms'
//       credits re-walked in the pass's order; the debits and the credits are
//       the same multiset of floats (sums equal bit for bit); a firm with no
//       positive profit pays nothing. Then a whole settle tick: each
//       `report.taxes` row's `collected` is the sum of its firms' `taxes`.
//   T2  DISCONTENT CLIMBS: from 0, tier V reaches the 0.5 threshold in exactly
//       4 quarters (a year) and bites in the 5th; tier IV in exactly 8 (two
//       years) and bites in the 9th; tier III holds for 40 quarters. Through
//       run_nation_taxes on a real nation, not only the pure function.
//   T3  THE PENALTY: 0 at the threshold, 15% at 1, 7.5% at 0.75; at
//       Discontent 1 a nation's ground's extraction draw and processing output
//       are 0.85 of the same world's at 0 (any owner), and a foreign nation's
//       ground is untouched.
//   T4  IT EBBS: tier I from 1 to 0 in 8 quarters, tier II from 0.5 to 0 in 8.
//   T5  THE SEAM: set_tax_tier through agent_protocol::service_line as the
//       seat — every malformed / out-of-range / unauthorised command rejected
//       with state_hash and every tier unchanged; a valid tier applies and moves
//       the hash; back to III restores it. A rival champion, in-process: refused.
//   T6  THE SAVE: v52 round-trips tiers and Discontent byte-identically with an
//       equal state_hash; a v51 stream (v52 less the tax section) loads with
//       every nation at III and Discontent 0; a tier 6, a tier 0, a NaN and a
//       1.5 Discontent each refuse the whole stream and leave the target world
//       untouched.
//   T7  THE SCORER: choose_tax_tier's rule rows; run_nation_tax_ai never moves
//       the seat's nation in a seated session, does under spectate, moves at
//       most one tier per nation per cadence.
//   T8  DETERMINISM: twelve spectating settle ticks run twice from one world —
//       equal state_hash every tick, equal tax rows and picks.
//
// Build: bash tools/verify/build_lua_harness.sh tax_discontent_harness
// Run:   build_gen/verify/tax_discontent_harness.exe [seed]   (default 0)

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/campaign_settle.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/nation_ai.hpp"
#include "world/nation_step.hpp"
#include "world/nation_tax.hpp"
#include "world/spawn_seat.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include "../../src/core/agent_protocol.cpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
int g_pass = 0;

void check(bool ok, const char* row, const std::string& what)
{
    std::printf("  [%s] %s %s\n", ok ? "PASS" : "FAIL", row, what.c_str());
    if (ok) ++g_pass; else ++g_fail;
}

std::string fmt(const char* f, ...)
{
    char buf[800];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

std::string serve(world& w, const recipe_registry& reg, const agent_protocol::session& s,
                  const std::string& line)
{
    std::string out;
    agent_protocol::service_line(line, w, reg, w.current_econ_tick, s, out);
    return out;
}

bool result_is(const std::string& out, const char* r)
{
    return out.find(std::string("result=") + r + " ") != std::string::npos
        || out.find(std::string("result=") + r + "\n") != std::string::npos;
}

std::string to_bytes(const world& w)
{
    std::ostringstream os(std::ios::binary);
    write_world_snapshot(w, os);
    return os.str();
}

bool from_bytes(const std::string& b, world& w)
{
    std::istringstream is(b, std::ios::binary);
    return read_world_snapshot(w, is);
}

std::vector<entity_id> nation_ids(const world& w)
{
    std::vector<entity_id> ids;
    for (const auto& kv : w.nations) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

/// Nations whose tax state differs from the default — the v52 section's rows.
std::size_t moved_nations(const world& w)
{
    std::size_t n = 0;
    for (const auto& [nid, nc] : w.nations)
        if (nc.tax_tier != k_default_tax_tier || nc.discontent != 0.0f) ++n;
    return n;
}

int bite_quarter(int tier, const tax_params& p, float* at_threshold_quarter_d, int* reach_q)
{
    float d = 0.0f;
    *reach_q = -1;
    for (int q = 1; q <= 80; ++q)
    {
        d = discontent_after_quarter(d, tier, p);
        if (*reach_q < 0 && d >= p.bite_threshold) { *reach_q = q; *at_threshold_quarter_d = discontent_penalty(d, p); }
        if (discontent_penalty(d, p) > 0.0f) return q;
    }
    return -1;
}

// ---------------------------------------------------------------------------

void t1_conserves(const app_start_world& out)
{
    std::printf("--- T1 the tax conserves ---\n");
    const tax_params& p = out.reg.tax();
    // One real tick for real money-loop budgets.
    world probe = out.w;
    settle_tick_result tick = run_settle_tick(probe, out.reg, probe.current_econ_tick + 1, 0, true);

    // Re-run the tax alone, on the PRE-tick world, with that tick's budgets less their taxes.
    world w = out.w;
    economy_report rep;
    rep.budgets = tick.report.budgets;
    for (auto& [cid, b] : rep.budgets) { b.taxes = 0.0f; b.subsidies = 0.0f; }
    std::map<entity_id, float> bal0, tre0;
    for (const auto& [cid, c] : w.corporations) bal0[cid] = c.balance;
    for (const auto& [nid, n] : w.nations) tre0[nid] = n.treasury;
    const economy_report before = rep;
    run_nation_taxes(w, p, rep);

    bool per_firm = true, nonpos_zero = true;
    int taxed = 0, untaxed = 0;
    std::map<entity_id, float> credit_rewalk = tre0;
    std::vector<float> debits;
    for (const auto& [cid, b] : before.budgets) // ascending corp id: the pass's order
    {
        const auto cit = w.corporations.find(cid);
        if (cit == w.corporations.end()) continue;
        const entity_id hn = cit->second.home_nation;
        const float profit = b.income - b.expenditure - b.maintenance - b.wages - b.interest - b.levies - b.upkeep;
        const float paid = rep.budgets.at(cid).taxes;
        if (w.nations.count(hn) == 0 || !(profit > 0.0f))
        {
            if (paid != 0.0f || cit->second.balance != bal0.at(cid)) nonpos_zero = false;
            ++untaxed;
            continue;
        }
        const float expect = profit * p.rate[tax_tier_index(w.nations.at(hn).tax_tier)];
        if (!(expect > 0.0f)) { ++untaxed; continue; }
        ++taxed;
        float bal = bal0.at(cid);
        bal -= expect;
        if (paid != expect || cit->second.balance != bal) per_firm = false;
        credit_rewalk[hn] += expect;
        debits.push_back(expect);
    }
    bool treasuries = true;
    for (const auto& [nid, n] : w.nations)
        if (n.treasury != credit_rewalk.at(nid)) treasuries = false;
    double sum_debit = 0.0, sum_credit = 0.0;
    for (const float d : debits) sum_debit += d;
    for (const nation_tax_row& r : rep.taxes) sum_credit += r.collected; // rows sum the same floats per nation
    double sum_row_firms = 0.0;
    for (const nation_tax_row& r : rep.taxes) sum_row_firms += r.firms_taxed;
    check(taxed > 0, "T1", fmt("the shipped world's first tick taxes %d firms (%d untaxed: no positive profit or no home nation)", taxed, untaxed));
    check(per_firm, "T1", "every taxed firm paid exactly profit x its home nation's tier rate, off its balance, booked on budgets.taxes");
    check(nonpos_zero, "T1", "a firm with no positive operating profit pays nothing");
    check(treasuries, "T1", "every treasury moved by exactly its firms' credits, re-walked in ascending corp order");
    check(std::fabs(sum_debit - sum_credit) <= 1e-6 * std::max(1.0, sum_debit) && sum_row_firms == taxed,
          "T1", fmt("debits %.4f = credits %.4f (a transfer, never a mint)", sum_debit, sum_credit));

    // The whole settle tick: rows agree with the firms' booked taxes.
    std::map<entity_id, double> by_nation;
    for (const auto& [cid, b] : tick.report.budgets)
        if (const auto cit = probe.corporations.find(cid); cit != probe.corporations.end())
            by_nation[cit->second.home_nation] += b.taxes;
    bool rows_ok = !tick.report.taxes.empty();
    double total = 0.0;
    for (const nation_tax_row& r : tick.report.taxes)
    {
        total += r.collected;
        if (std::fabs(static_cast<double>(r.collected) - by_nation[r.nation]) > 1e-3 * std::max(1.0, by_nation[r.nation]))
            rows_ok = false;
    }
    check(rows_ok, "T1", fmt("a whole settle tick: %zu nation rows, each `collected` = its firms' booked taxes (%.1f credits in all)",
                             tick.report.taxes.size(), total));
}

void t2_t4_discontent(const app_start_world& out)
{
    std::printf("--- T2 Discontent climbs; T4 it ebbs ---\n");
    const tax_params& p = out.reg.tax();
    float pen_at = -1.0f;
    int reach = 0;
    const int v_bite = bite_quarter(5, p, &pen_at, &reach);
    check(reach == 4 && pen_at == 0.0f && v_bite == 5, "T2",
          fmt("tier V: threshold in %d quarters (penalty there %.3f), bites in quarter %d", reach, pen_at, v_bite));
    const int iv_bite = bite_quarter(4, p, &pen_at, &reach);
    check(reach == 8 && pen_at == 0.0f && iv_bite == 9, "T2",
          fmt("tier IV: threshold in %d quarters (two years), bites in quarter %d", reach, iv_bite));
    {
        float d = 0.0f;
        for (int q = 0; q < 40; ++q) d = discontent_after_quarter(d, 3, p);
        float d2 = 0.3f;
        for (int q = 0; q < 40; ++q) d2 = discontent_after_quarter(d2, 3, p);
        check(d == 0.0f && d2 == 0.3f, "T2", "tier III holds Discontent for 40 quarters (0 stays 0, 0.3 stays 0.3)");
    }
    // Through the pass, on a real nation.
    {
        world w = out.w;
        const std::vector<entity_id> ids = nation_ids(w);
        const entity_id a = ids.front(), b = ids.back();
        w.nations.at(a).tax_tier = 5;
        w.nations.at(b).tax_tier = 4;
        economy_report rep;
        for (int q = 0; q < 4; ++q) { rep.taxes.clear(); run_nation_taxes(w, p, rep); }
        const float da4 = w.nations.at(a).discontent, db4 = w.nations.at(b).discontent;
        for (int q = 0; q < 4; ++q) { rep.taxes.clear(); run_nation_taxes(w, p, rep); }
        check(da4 == 0.5f && db4 == 0.25f && w.nations.at(a).discontent == 1.0f && w.nations.at(b).discontent == 0.5f,
              "T2", fmt("run_nation_taxes: nation %u at V reads %.3f after a year, %.3f after two (clamped); nation %u at IV %.3f then %.3f",
                        a, da4, w.nations.at(a).discontent, b, db4, w.nations.at(b).discontent));
        bool others_zero = true;
        for (const entity_id n : ids)
            if (n != a && n != b && w.nations.at(n).discontent != 0.0f) others_zero = false;
        check(others_zero, "T2", "every nation left at III holds Discontent 0");
    }
    std::printf("--- T4 ---\n");
    {
        float d = 1.0f;
        int q1 = 0;
        while (d > 0.0f && q1 < 100) { d = discontent_after_quarter(d, 1, p); ++q1; }
        float e = 0.5f;
        int q2 = 0;
        while (e > 0.0f && q2 < 100) { e = discontent_after_quarter(e, 2, p); ++q2; }
        check(q1 == 8 && q2 == 8, "T4", fmt("tier I ebbs 1 -> 0 in %d quarters; tier II ebbs 0.5 -> 0 in %d (the same pace as IV/V climb)", q1, q2));
        check(discontent_after_quarter(0.0f, 1, p) == 0.0f, "T4", "Discontent never goes below 0");
    }
}

void t3_penalty(const app_start_world& out)
{
    std::printf("--- T3 the penalty ---\n");
    const tax_params& p = out.reg.tax();
    check(discontent_penalty(0.5f, p) == 0.0f && discontent_penalty(0.0f, p) == 0.0f, "T3", "0 at and below the threshold");
    check(std::fabs(discontent_penalty(1.0f, p) - 0.15f) < 1e-7f, "T3", fmt("15%% at Discontent 1 (%.6f)", discontent_penalty(1.0f, p)));
    check(std::fabs(discontent_penalty(0.75f, p) - 0.075f) < 1e-7f, "T3", fmt("7.5%% at 0.75 (%.6f)", discontent_penalty(0.75f, p)));

    // Production: an extraction site on nation N's ground, at D 0 and D 1.
    const world& w0 = out.w;
    entity_id site = null_entity, nation = null_entity, owner = null_entity;
    for (const auto& [cid, c] : w0.corporations)
        for (const entity_id bid : c.assets)
        {
            const auto bit = w0.buildings.find(bid);
            if (bit == w0.buildings.end() || bit->second.type != building_type::extraction_site) continue;
            const auto tn = w0.tile_to_nation.find(bit->second.tile);
            if (tn == w0.tile_to_nation.end()) continue;
            if (extraction_nominal(w0, out.reg, bit->second, 1.0f, cid) <= 0.0f) continue;
            if (site == null_entity || bid < site) { site = bid; nation = tn->second; owner = cid; }
        }
    if (site == null_entity) { check(false, "T3", "an extraction site on a nation's ground"); return; }
    world w1 = w0;
    w1.nations.at(nation).discontent = 1.0f;
    const building_component& b = w0.buildings.at(site);
    const float q0 = extraction_nominal(w0, out.reg, b, 1.0f, owner);
    const float q1 = extraction_nominal(w1, out.reg, b, 1.0f, owner);
    check(std::fabs(q1 / q0 - 0.85f) < 1e-5f, "T3",
          fmt("extraction site %u (owner %u) on nation %u: draw %.4f at D 0, %.4f at D 1 (x%.5f)", site, owner, nation, q0, q1, q1 / q0));
    // Any owner: the same site under a different owner's id still pays its ground's cut.
    entity_id other_owner = null_entity;
    for (const auto& [cid, c] : w0.corporations)
        if (cid != owner && c.home_nation != nation) { other_owner = cid; break; }
    if (other_owner != null_entity)
    {
        const float r0 = extraction_nominal(w0, out.reg, b, 1.0f, other_owner);
        const float r1 = extraction_nominal(w1, out.reg, b, 1.0f, other_owner);
        check(r0 > 0.0f && std::fabs(r1 / r0 - 0.85f) < 1e-5f, "T3",
              fmt("the ground's cut binds a foreign owner too (corp %u: x%.5f)", other_owner, r0 > 0.0f ? r1 / r0 : 0.0f));
    }
    // Another nation's Discontent does not touch this ground.
    {
        world w2 = w0;
        for (auto& [nid, nc] : w2.nations) if (nid != nation) nc.discontent = 1.0f;
        const float q2 = extraction_nominal(w2, out.reg, b, 1.0f, owner);
        check(q2 == q0, "T3", "every OTHER nation at Discontent 1 leaves this ground's draw bit-identical");
    }
    // Processing output at the one accrual point.
    {
        const recipe* rc = nullptr;
        for (std::size_t i = 0; i < out.reg.recipe_count(); ++i)
        {
            const recipe* rp = out.reg.get_recipe(static_cast<uint16_t>(i));
            if (rp == nullptr) continue;
            const recipe& r = *rp;
            bool capped = false, any = false;
            for (std::size_t g = 0; g < resource_count; ++g)
                if (r.outputs[g] > 0.0f) { any = true; if (out.reg.grid_goods().ceiling(g) > 0.0f || out.reg.grid_goods().grid(g)) capped = true; }
            if (any && !capped) { rc = &r; break; }
        }
        entity_id mkt = null_entity;
        for (const auto& [mid, m] : w0.markets) if (mkt == null_entity || mid < mkt) mkt = mid;
        if (rc != nullptr && mkt != null_entity)
        {
            world a = w0, c = w1;
            const float o0 = credit_processing_outputs(a, out.reg, owner, mkt, *rc, 2.0f, b.tile);
            const float o1 = credit_processing_outputs(c, out.reg, owner, mkt, *rc, 2.0f, b.tile);
            const float on = credit_processing_outputs(c, out.reg, owner, mkt, *rc, 2.0f); // no ground named
            check(o0 > 0.0f && std::fabs(o1 / o0 - 0.85f) < 1e-5f && on == o0, "T3",
                  fmt("processing '%s' on that ground: %.4f at D 0, %.4f at D 1 (x%.5f); no ground named -> %.4f",
                      rc->name.c_str(), o0, o1, o0 > 0.0f ? o1 / o0 : 0.0f, on));
        }
        else
            check(false, "T3", "a processing recipe and a market to land on");
    }
}

void t5_seam(app_start_world& out)
{
    std::printf("--- T5 the seam ---\n");
    world& w = out.w;
    const recipe_registry& reg = out.reg;
    const entity_id P  = w.player_entity;
    const entity_id pn = player_nation(w);
    if (pn == null_entity || champion_of(w, pn) != P) { check(false, "T5", "the seat is its major nation's champion"); return; }
    agent_protocol::session ps;
    ps.actor = P;
    const unsigned V = static_cast<unsigned>(corp_verb::set_tax_tier);
    const auto P_ = [&](const std::string& rest) { return fmt("COMMAND corp=%u verb=%u ", static_cast<unsigned>(P), V) + rest; };
    entity_id other_n = null_entity;
    for (const entity_id n : nation_ids(w)) if (n != pn) { other_n = n; break; }
    struct rej { std::string line; const char* want; const char* what; };
    std::vector<rej> rows = {
        {P_("order=4"), "rejected_invalid", "no nation (a corporation's book)"},
        {P_(fmt("nation=%u order=0", pn)), "rejected_invalid", "tier 0"},
        {P_(fmt("nation=%u order=6", pn)), "rejected_invalid", "tier 6"},
        {P_(fmt("nation=%u order=4294967295", pn)), "rejected_invalid", "tier UINT32_MAX"},
        {P_(fmt("nation=%u order=-1", pn)), "rejected_invalid", "a negative tier"},
        {P_(fmt("nation=%u order=abc", pn)), "rejected_invalid", "a malformed tier"},
        {P_(fmt("nation=%u order=1e10", pn)), "rejected_invalid", "a tier past uint32"},
        {P_("nation=abc order=4"), "rejected_invalid", "a malformed nation id"},
        {P_(fmt("nation=%u", pn)), "rejected_invalid", "no tier (order 0)"},
    };
    if (other_n != null_entity)
        rows.push_back({P_(fmt("nation=%u order=4", other_n)), "rejected_not_owner", "another nation's tier (actor gate)"});
    std::vector<int> tiers0;
    for (const entity_id n : nation_ids(w)) tiers0.push_back(w.nations.at(n).tax_tier);
    bool all_rej = true;
    for (const rej& r : rows)
    {
        const std::uint64_t h0 = w.state_hash(0);
        const std::string o = serve(w, reg, ps, r.line);
        std::vector<int> t;
        for (const entity_id n : nation_ids(w)) t.push_back(w.nations.at(n).tax_tier);
        // A refusal by the WIRE parser (a malformed field) answers ERR rather
        // than a result; either way nothing may move.
        const bool refused = result_is(o, r.want) || o.rfind("ERR", 0) == 0;
        const bool ok = refused && !result_is(o, "applied") && w.state_hash(0) == h0 && t == tiers0;
        if (!ok) { all_rej = false; std::printf("    REJECT ROW FAILED: %s -> %s", r.what, o.c_str()); }
    }
    check(all_rej, "T5", fmt("%zu malformed / out-of-range / unauthorised tax commands refused, state_hash and every tier unchanged", rows.size()));
    const std::uint64_t h0 = w.state_hash(0);
    const int was = w.nations.at(pn).tax_tier;
    const std::string o1 = serve(w, reg, ps, P_(fmt("nation=%u order=5", pn)));
    check(result_is(o1, "applied") && w.nations.at(pn).tax_tier == 5 && w.state_hash(0) != h0, "T5",
          fmt("a valid tier applies (%d -> V) and moves the state hash", was));
    const std::string o2 = serve(w, reg, ps, P_(fmt("nation=%u order=%d", pn, was)));
    check(result_is(o2, "applied") && w.nations.at(pn).tax_tier == was && w.state_hash(0) == h0, "T5",
          "setting it back restores the tier and the hash");
    // A rival champion, in-process: refused (the AI's tier is its scorer's, never corp_ai's).
    entity_id rn = null_entity;
    for (const entity_id n : nation_ids(w)) if (n != pn && is_major_nation(w, n) && champion_of(w, n) != null_entity) { rn = n; break; }
    if (rn != null_entity)
    {
        corp_command cmd;
        cmd.corp = champion_of(w, rn); cmd.as_nation = rn; cmd.verb = corp_verb::set_tax_tier; cmd.order = 4;
        const std::uint64_t h1 = w.state_hash(0);
        const corp_command_result r = apply_corp_command(w, reg, cmd);
        check(r == corp_command_result::rejected_not_owner && w.state_hash(0) == h1, "T5",
              "a rival champion may not set its nation's tier through the command seam");
    }
    // The verb without a nation, in-process.
    {
        corp_command cmd;
        cmd.corp = P; cmd.verb = corp_verb::set_tax_tier; cmd.order = 4;
        const std::uint64_t h1 = w.state_hash(0);
        check(apply_corp_command(w, reg, cmd) == corp_command_result::rejected_invalid && w.state_hash(0) == h1,
              "T5", "set_tax_tier on a corporation's own book is rejected_invalid");
    }
}

void t6_save(const app_start_world& out)
{
    std::printf("--- T6 the save ---\n");
    world c = out.w;
    const std::vector<entity_id> ids = nation_ids(c);
    c.nations.at(ids.front()).tax_tier   = 5;
    c.nations.at(ids.front()).discontent = 0.6875f;
    c.nations.at(ids.back()).tax_tier    = 1;
    c.nations.at(ids[ids.size() / 2]).discontent = 0.125f; // tier III, Discontent moved
    const std::string b52 = to_bytes(c);
    world r;
    const bool rt = from_bytes(b52, r);
    check(rt && to_bytes(r) == b52 && r.state_hash(0) == c.state_hash(0)
              && r.nations.at(ids.front()).tax_tier == 5 && r.nations.at(ids.front()).discontent == 0.6875f
              && r.nations.at(ids.back()).tax_tier == 1 && r.nations.at(ids[ids.size() / 2]).discontent == 0.125f,
          "T6", fmt("v%u round-trips tiers and Discontent byte-identically (%zu bytes) with an equal state_hash",
                    static_cast<unsigned>(world_save_version), b52.size()));

    // v51: the stream less the tax section (u32 count + 9 bytes a moved nation).
    const std::size_t tail = 4 + 9 * moved_nations(c);
    std::string b51 = b52.substr(0, b52.size() - tail);
    const std::uint32_t v51 = k_world_save_version_pre_tax;
    std::memcpy(&b51[4], &v51, 4);
    world r51;
    const bool ok51 = from_bytes(b51, r51);
    bool defaults = ok51;
    if (ok51)
        for (const auto& [nid, nc] : r51.nations)
            if (nc.tax_tier != k_default_tax_tier || nc.discontent != 0.0f) defaults = false;
    check(defaults, "T6", "a v51 stream loads, every nation at tier III with Discontent 0");
    world base = out.w;
    check(ok51 && r51.state_hash(0) == base.state_hash(0), "T6",
          "...and hashes as the default world (the fold is sparse)");

    // Bad values refuse whole and leave the target untouched. The FIRST row of
    // the section is ids.front(): id (4) then tier (1) then discontent (4).
    const std::size_t row0 = b52.size() - tail + 4;
    const auto refuse = [&](const char* what, auto patch) {
        std::string bad = b52;
        patch(bad);
        world tgt = out.w;
        const std::uint64_t h = tgt.state_hash(0);
        const bool read = from_bytes(bad, tgt);
        check(!read && tgt.state_hash(0) == h, "T6", fmt("%s refuses the whole stream; the target world is untouched", what));
    };
    refuse("a tier 6", [&](std::string& s) { s[row0 + 4] = static_cast<char>(6); });
    refuse("a tier 0", [&](std::string& s) { s[row0 + 4] = static_cast<char>(0); });
    refuse("a NaN Discontent", [&](std::string& s) { const float f = std::numeric_limits<float>::quiet_NaN(); std::memcpy(&s[row0 + 5], &f, 4); });
    refuse("a Discontent of 1.5", [&](std::string& s) { const float f = 1.5f; std::memcpy(&s[row0 + 5], &f, 4); });
    refuse("a negative Discontent", [&](std::string& s) { const float f = -0.25f; std::memcpy(&s[row0 + 5], &f, 4); });
    refuse("an unknown nation id", [&](std::string& s) { const std::uint32_t id = 0x7FFFFFF0u; std::memcpy(&s[row0], &id, 4); });
}

void t7_scorer(const app_start_world& out)
{
    std::printf("--- T7 the scorer ---\n");
    const tax_params& p = out.reg.tax();
    const int k = out.reg.nation_ai().cadence_k;
    struct row { int tier; float d; bool sh; int want; const char* what; };
    const row rows[] = {
        {3, 0.0f,    true,  4, "short, Discontent 0: III -> IV"},
        {4, 0.25f,   true,  3, "at IV with 0.25: a year more passes the limit -> III"},
        {3, 0.25f,   true,  3, "short at III with 0.25: IV would pass the limit -> hold"},
        {3, 0.0f,    false, 3, "not short at III: hold"},
        {5, 0.0f,    false, 4, "not short at V: -> IV"},
        {2, 0.0f,    false, 3, "not short at II: back to III"},
        {3, 0.4375f, false, 3, "at the limit at III: hold"},
        {3, 0.45f,   true,  2, "past the limit at III: -> II (ebb)"},
        {1, 0.9f,    true,  1, "at I: never below I"},
        {5, 0.0f,    true,  4, "at V with 0: a year at V passes the limit -> IV"},
    };
    bool all = true;
    for (const row& r : rows)
    {
        const int got = choose_tax_tier(r.tier, r.d, r.sh, p, k);
        if (got != r.want) { all = false; std::printf("    ROW FAILED: %s: got %d want %d\n", r.what, got, r.want); }
    }
    check(all, "T7", fmt("%zu choose_tax_tier rule rows (cadence %d)", sizeof rows / sizeof rows[0], k));

    // Every nation short (a rationed line), every tick of one cadence.
    world w = out.w;
    const entity_id pn = player_nation(w);
    economy_report rep;
    for (const entity_id n : nation_ids(w))
    {
        nation_budget_result r;
        r.nation = n;
        r.lines[0].demand = 10.0f;
        r.lines[0].rationed = true;
        rep.national_budget.nations.push_back(r);
    }
    std::map<entity_id, int> t0;
    for (const entity_id n : nation_ids(w)) t0[n] = w.nations.at(n).tax_tier;
    std::map<entity_id, int> picks;
    for (int t = 0; t < k; ++t)
    {
        std::vector<tax_ai_pick> out_p;
        run_nation_tax_ai(w, out.reg, rep, t, /*spectating=*/false, &out_p);
        for (const tax_ai_pick& pk : out_p) ++picks[pk.nation];
    }
    bool once = true, others_moved = true;
    for (const entity_id n : nation_ids(w))
    {
        if (n == pn) continue;
        if (picks[n] != 1) once = false;
        if (std::abs(w.nations.at(n).tax_tier - t0[n]) > 1) once = false;
        if (w.nations.at(n).tax_tier != 4) others_moved = false;
    }
    check(pn != null_entity && w.nations.at(pn).tax_tier == t0[pn] && picks.count(pn) == 0, "T7",
          "seated: the seat's nation is never scored and its tier never moves");
    check(once && others_moved, "T7", "every other nation is scored once a cadence and steps exactly one tier (III -> IV, short)");
    world s = out.w;
    for (int t = 0; t < k; ++t) run_nation_tax_ai(s, out.reg, rep, t, /*spectating=*/true);
    check(s.nations.at(pn).tax_tier == 4, "T7", "under spectate the seat's nation is scored like any nation (III -> IV)");
}

void t8_determinism(const app_start_world& out)
{
    std::printf("--- T8 determinism ---\n");
    world a = out.w, b = out.w;
    bool same = true;
    int moved = 0, taxed_rows = 0;
    double collected = 0.0;
    const int e0 = out.w.current_econ_tick + 1;
    for (int k = 0; k < 12 && same; ++k)
    {
        const settle_tick_result ra = run_settle_tick(a, out.reg, e0 + k, 0, true);
        const settle_tick_result rb = run_settle_tick(b, out.reg, e0 + k, 0, true);
        if (a.state_hash(0) != b.state_hash(0)) same = false;
        if (ra.report.taxes.size() != rb.report.taxes.size() || ra.report.tax_picks.size() != rb.report.tax_picks.size())
            same = false;
        for (std::size_t i = 0; same && i < ra.report.taxes.size(); ++i)
            if (ra.report.taxes[i].collected != rb.report.taxes[i].collected
                || ra.report.taxes[i].discontent_to != rb.report.taxes[i].discontent_to)
                same = false;
        for (std::size_t i = 0; same && i < ra.report.tax_picks.size(); ++i)
        {
            if (ra.report.tax_picks[i].tier_to != rb.report.tax_picks[i].tier_to) same = false;
            if (ra.report.tax_picks[i].tier_to != ra.report.tax_picks[i].tier_from) ++moved;
        }
        for (const nation_tax_row& r : ra.report.taxes) { collected += r.collected; if (r.firms_taxed > 0) ++taxed_rows; }
    }
    std::map<int, int> hist;
    for (const auto& [nid, nc] : a.nations) ++hist[nc.tax_tier];
    std::string h;
    for (const auto& [t, n] : hist) h += fmt(" tier %d: %d", t, n);
    check(same, "T8", fmt("twelve spectating ticks twice: equal state_hash every tick, equal tax rows and picks "
                          "(%d tier moves, %.1f credits taxed, final%s)", moved, collected, h.c_str()));
}

} // namespace

int main(int argc, char** argv)
{
    std::uint32_t seed = 0;
    if (argc > 1) seed = static_cast<std::uint32_t>(std::strtoul(argv[1], nullptr, 10));
    lua_state lua;
    std::printf("=== tax_discontent_harness seed %u ===\n", static_cast<unsigned>(seed));
    world_params params = arc_params(world_arc::shipped);
    params.seed = seed;
    app_start_world out;
    build_app_start_world(lua, params, out);
    (void)seat_player_corporation(out.w, seed, out.land.search.winner_score);
    const tax_params& p = out.reg.tax();
    std::printf("dials: rates %.2f %.2f %.2f %.2f %.2f | pressure %.4f %.4f %.4f %.4f %.4f | threshold %.3f | max penalty %.3f | ai headroom %.4f\n",
                p.rate[0], p.rate[1], p.rate[2], p.rate[3], p.rate[4], p.pressure[0], p.pressure[1], p.pressure[2],
                p.pressure[3], p.pressure[4], p.bite_threshold, p.max_penalty, p.ai_headroom);
    std::printf("world: %zu nations, %zu corporations, seat %u (nation %u)\n", out.w.nations.size(),
                out.w.corporations.size(), out.w.player_entity, player_nation(out.w));

    t1_conserves(out);
    t2_t4_discontent(out);
    t3_penalty(out);
    t6_save(out);
    t7_scorer(out);
    t8_determinism(out);
    t5_seam(out); // last: it mutates out.w (and restores it)

    std::printf("\ntax_discontent_harness: %d pass, %d fail -- %s\n", g_pass, g_fail, g_fail == 0 ? "ALL PASS" : "FAILURES");
    return g_fail == 0 ? 0 : 1;
}
