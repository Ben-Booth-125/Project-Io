// ---------------------------------------------------------------------------
// state_industry_harness — BL-1296 (state industry), the headless proof
// ---------------------------------------------------------------------------
// docs/politics/NATIONS.md § State industry (Ben, 2026-10-11/13). On the
// SHIPPED start (harness_params.hpp's app mirror), per seed:
//
//   S1  THE HANDOVER. The landscape is applied with the handover held back,
//       every corporation's holdings are snapshotted, then
//       `hand_over_state_industry` runs:
//         S1a every MAJOR owns at least one state building (reported per nation);
//         S1b no MINOR owns any;
//         S1c every state building exists, stands on its owner's ground, is a
//             strategic building the state runs, is owned by no corporation and
//             listed once — no dangling or doubled owner ids;
//         S1d every CHAMPION (and every specialist) kept exactly its holdings;
//         S1e a background firm lost only what passed to a nation, and every
//             state building came from a background firm's holdings;
//         S1f a second handover passes nothing (idempotent).
//   S2  THE SETTLE (12 ticks, spectate, run_settle_tick as the app runs it):
//       no state treasury ever below zero; the ledger opens a row per nation.
//   S3  MONEY THROUGH THE TREASURY, one live tick orchestrated by hand around
//       each phase: the economy step moves a state treasury by exactly its
//       flat construction (and any state retool); the clear moves no treasury;
//       apply_budget moves it by exactly the state ledger's market income less
//       its shelf purchases, maintenance and wages (plus enacted levies, none on
//       a fresh campaign) — every state market flow lands in the treasury and
//       nothing else does. No treasury ends below zero.
//   S4  AN UNPAID BUILDING IDLES. A copy of the world with one major's treasury
//       at zero: every one of its state buildings idles, produces nothing, and
//       pays nothing; its treasury does not go below zero. Refunded, they run.
//   S5  THE SEAM. Through agent_protocol::service_line with the player's corp
//       as the session actor: every malformed or unauthorised state command is
//       rejected and leaves state_hash unchanged; a valid one applies.
//   S6  THE GRANT'S SCOPE. Eight live (not spectating) ticks: the scorer and the
//       reflex never touch the player's nation's state buildings, and no logged
//       decision names the player's nation.
//   S7  THE SAVE. v48 round-trips state_assets and state_idle with an equal
//       state_hash; a v47 stream (the v48 stream less its trailing section)
//       reads, as a world with no state industry.
//
//   S2b STATE BUILDS THROUGH THE VERBS. Whatever the handover left, each
//       major's champion builds a state plant through the legal verb (the
//       player's through the wire seam) and the world runs live until they
//       complete or 30 ticks pass — the fixture S3-S7 exercise is made of
//       legal commands; the gate's ledger must agree with the idle flags.
//
// Build: bash tools/verify/build_lua_harness.sh state_industry_harness
// Run:   build_gen/verify/state_industry_harness.exe [--strict] [seed ...]   (default: 0)
// Exits non-zero on any FAIL. S1a is a FAIL again since the founding (Ben, 2026-10-14);
// S1g checks the founding and S8 the post-gate changes; --strict covers finding() rows.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/budget_system.hpp"
#include "world/campaign_settle.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/nation_step.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/spawn_seat.hpp"
#include "world/state_industry.hpp"
#include "world/supply_system.hpp"
#include "world/trade.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

// The wire protocol is one TU in src/core that no headless builder compiles
// (CMake links it for agent_seam_harness). It reaches only world/* headers, so
// it is compiled INTO this TU: S5 drives the exact parser and actor gate the
// --serve and live-agent hosts run, not a restatement of them.
#include "../../src/core/agent_protocol.cpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
int g_pass = 0;
int g_finding = 0;
bool g_strict = false;

/// A measurement of the GENERATED world the design expects but the data may
/// not give (S1a): reported loudly, counted apart from the mechanics, and a
/// FAIL only under --strict — the mechanics rows below never depend on it.
void finding(bool ok, const char* row, const std::string& what)
{
    if (ok || g_strict)
    {
        std::printf("  [%s] %s %s\n", ok ? "PASS" : "FAIL", row, what.c_str());
        if (ok) ++g_pass; else ++g_fail;
        return;
    }
    std::printf("  [FINDING] %s %s\n", row, what.c_str());
    ++g_finding;
}

void check(bool ok, const char* row, const std::string& what)
{
    std::printf("  [%s] %s %s\n", ok ? "PASS" : "FAIL", row, what.c_str());
    if (ok) ++g_pass; else ++g_fail;
}

std::string fmt(const char* f, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

bool near(float a, float b, float scale)
{
    return std::fabs(a - b) <= 1e-2f + 2e-5f * std::max(1.0f, std::fabs(scale));
}

std::vector<entity_id> sorted_nations(const world& w)
{
    std::vector<entity_id> ids;
    for (const auto& kv : w.nations) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<entity_id> state_nations(const world& w)
{
    std::vector<entity_id> ids;
    for (const auto& [nid, nc] : w.nations)
        if (!nc.state_assets.empty()) ids.push_back(nid);
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::map<entity_id, float> treasuries(const world& w)
{
    std::map<entity_id, float> out;
    for (const auto& [nid, nc] : w.nations) out[nid] = nc.treasury;
    return out;
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

// ---------------------------------------------------------------------------

void run_seed(lua_state& lua, std::uint32_t seed)
{
    std::printf("\n=== seed %u ===\n", static_cast<unsigned>(seed));
    world_params params = arc_params(world_arc::shipped);
    params.seed = seed;

    app_start_world out;
    build_app_base_world(lua, params, out);
    harness_charter_input ci;
    ci.skip_state_handover = true;
    apply_app_start_landscape(out, ci);
    world& w = out.w;
    const recipe_registry& reg = out.reg;

    // ---- S1: the handover -------------------------------------------------
    std::printf("--- S1 the handover ---\n");
    std::map<entity_id, std::vector<entity_id>> before;
    for (const auto& [cid, cc] : w.corporations) before[cid] = cc.assets;
    bool any_state_before = false;
    for (const auto& [nid, nc] : w.nations) any_state_before |= !nc.state_assets.empty();
    check(!any_state_before, "S1", "no nation owns a building before the handover");

    {
        // The ground the handover reads: buildings by owner kind x sector x the
        // ground's nation kind (reported, so a zero row explains itself).
        std::map<std::string, int> tally;
        for (const auto& [cid, cc] : w.corporations)
            for (const entity_id bid : cc.assets)
            {
                const auto bit = w.buildings.find(bid);
                if (bit == w.buildings.end()) continue;
                const auto tn = w.tile_to_nation.find(bit->second.tile);
                const char* ground = (tn == w.tile_to_nation.end()) ? "unclaimed"
                                   : is_major_nation(w, tn->second) ? "major" : "minor";
                const strategic_sector sec = strategic_sector_of(reg, bit->second);
                ++tally[fmt("%s %s type%d %s", cc.is_background ? "firm" : "spec", ground,
                            static_cast<int>(bit->second.type), strategic_sector_name(sec))];
            }
        for (const auto& [k, n] : tally)
            std::printf("    ground: %-40s %d\n", k.c_str(), n);
    }
    const state_handover_report rep = hand_over_state_industry(w, reg);
    print_state_handover(rep);

    int majors = 0, minors = 0, majors_with = 0, minors_with = 0, total_state = 0;
    for (const entity_id nid : sorted_nations(w))
    {
        const nation_component& nc = w.nations.at(nid);
        const bool major = is_major_nation(w, nid);
        (major ? majors : minors) += 1;
        if (!nc.state_assets.empty()) (major ? majors_with : minors_with) += 1;
        total_state += static_cast<int>(nc.state_assets.size());
        if (major)
        {
            std::array<int, 6> sec{};
            for (const entity_id bid : nc.state_assets)
                ++sec[static_cast<std::size_t>(strategic_sector_of(reg, w.buildings.at(bid)))];
            std::printf("    major %u \"%s\": %zu state building(s) (energy %d, steel %d, arms %d, "
                        "space %d), treasury %.1f\n",
                        static_cast<unsigned>(nid), nc.name.c_str(), nc.state_assets.size(),
                        sec[1], sec[2], sec[3], sec[5], static_cast<double>(nc.treasury));
        }
    }
    std::printf("    COUNTS seed %u: nations %d (majors %d, minors %d), state buildings %d\n",
                static_cast<unsigned>(seed), majors + minors, majors, minors, total_state);
    check(majors > 0 && majors_with == majors, "S1a",
            fmt("every major owns state industry after the handover (%d of %d)", majors_with, majors));
    check(minors_with == 0, "S1b", fmt("no minor owns state industry (%d minors)", minors));

    {
        std::set<entity_id> corp_owned;
        for (const auto& [cid, cc] : w.corporations)
            for (const entity_id b : cc.assets) corp_owned.insert(b);
        std::set<entity_id> seen;
        bool ok = true;
        std::string why;
        for (const auto& [nid, nc] : w.nations)
            for (const entity_id bid : nc.state_assets)
            {
                const auto bit = w.buildings.find(bid);
                if (bit == w.buildings.end()) { ok = false; why = "dangling id"; continue; }
                const auto tn = w.tile_to_nation.find(bit->second.tile);
                if (tn == w.tile_to_nation.end() || tn->second != nid) { ok = false; why = "off its ground"; }
                if (!state_runs(reg, bit->second.type, bit->second.recipe)) { ok = false; why = "not a state sector"; }
                if (corp_owned.count(bid)) { ok = false; why = "also corp-owned"; }
                if (!seen.insert(bid).second) { ok = false; why = "listed twice"; }
                if (building_owner_of(w, bid).kind != owner_kind::nation
                    || owner_nation_of(w, bid) != nid) { ok = false; why = "owner lookup disagrees"; }
            }
        check(ok, "S1c", "every state building exists, on its nation's ground, strategic, owned once"
                         + (ok ? std::string() : " -- " + why));
    }
    {
        bool champ_ok = true, firm_ok = true;
        std::set<entity_id> passed, founded_ids;
        for (const state_founding& f : rep.founded)
            if (f.building != null_entity) founded_ids.insert(f.building);
        for (const auto& [nid, nc] : w.nations)
            for (const entity_id b : nc.state_assets)
                if (!founded_ids.count(b)) passed.insert(b);
        std::set<entity_id> lost_all;
        for (const auto& [cid, cc] : w.corporations)
        {
            const std::vector<entity_id>& was = before.at(cid);
            if (!cc.is_background)
            {
                if (cc.assets != was) champ_ok = false;
                continue;
            }
            std::set<entity_id> now(cc.assets.begin(), cc.assets.end());
            for (const entity_id b : was)
                if (!now.count(b))
                {
                    lost_all.insert(b);
                    if (!passed.count(b)) firm_ok = false;
                }
        }
        check(champ_ok, "S1d", "every champion and specialist kept exactly its holdings");
        check(firm_ok && lost_all == passed, "S1e",
              fmt("background firms lost only what passed, and all of it came from them (%zu)",
                  passed.size()));
        // S1g: the founding. Every major the handover left empty founded one
        // plant, complete, on its own ground, paid from its treasury (never
        // more than it held), in the history log; chain-feasible where any is.
        int founded = 0, infeasible = 0, overdrawn = 0, logged = 0;
        for (const state_founding& f : rep.founded)
        {
            if (f.building == null_entity) continue;
            ++founded;
            if (!f.chain_feasible) ++infeasible;
            if (w.nations.at(f.nation).treasury < 0.0f || f.paid > f.price) ++overdrawn;
            if (w.buildings.at(f.building).ticks_remaining != 0) ++overdrawn;
        }
        for (const world_history_entry& e : w.history_log)
            if (e.event.rfind("State charter: ", 0) == 0) ++logged;
        check(founded == static_cast<int>(rep.founded.size()) && logged >= founded && overdrawn == 0, "S1g",
              fmt("%d major(s) founded a plant (%d not chain-feasible), complete, paid within the treasury, "
                  "recorded as a state charter", founded, infeasible));
        for (const state_founding& f : rep.founded)
            if (f.building != null_entity)
                std::printf("    founded: nation %u -> %s at building %u, price %.1f paid %.1f%s\n",
                            static_cast<unsigned>(f.nation), reg.get_recipe(f.recipe)->name.c_str(),
                            static_cast<unsigned>(f.building), static_cast<double>(f.price),
                            static_cast<double>(f.paid), f.chain_feasible ? "" : " (NOT chain-feasible)");
    }
    {
        const std::uint64_t h0 = w.state_hash(0);
        const state_handover_report again = hand_over_state_industry(w, reg);
        check(again.passed == 0 && w.state_hash(0) == h0, "S1f", "a second handover passes nothing");
    }

    // ---- S2: the settle ---------------------------------------------------
    std::printf("--- S2 the settle ---\n");
    bool never_negative = true, ledger_rows = true;
    for (int k = 0; k < k_app_validation_ticks; ++k)
    {
        run_app_validation_tick(w, reg, k);
        for (const entity_id nid : state_nations(w))
        {
            if (w.nations.at(nid).treasury < 0.0f) never_negative = false;
            if (!w.state_ledger.count(nid)) ledger_rows = false;
        }
    }
    clear_research_at_handover(w);
    check(never_negative, "S2", "no state treasury below zero through the 12-tick settle");
    check(ledger_rows, "S2", "the funding gate opened a ledger row for every state nation, every tick");
    for (const entity_id nid : state_nations(w))
    {
        const world::state_tick_ledger& L = w.state_ledger.at(nid);
        std::printf("    nation %u: funded %d idled %d  income %.1f  bought %.1f  maint %.1f  wages %.1f"
                    "  build %.1f  treasury %.1f -> %.1f\n",
                    static_cast<unsigned>(nid), L.funded, L.idled, static_cast<double>(L.income),
                    static_cast<double>(L.expenditure), static_cast<double>(L.maintenance),
                    static_cast<double>(L.wages), static_cast<double>(L.construction),
                    static_cast<double>(L.treasury_open), static_cast<double>(L.treasury_close));
    }

    // The seat, as the app takes it after the settle.
    const spawn_seat_result seat = seat_player_corporation(w, seed, out.land.search.winner_score);
    (void)seat;
    const entity_id pn = player_nation(w);
    std::printf("    seat: player corp %u, nation %u (%zu state buildings)\n",
                static_cast<unsigned>(w.player_entity), static_cast<unsigned>(pn),
                w.nations.count(pn) ? w.nations.at(pn).state_assets.size() : 0u);
    int next_tick = k_app_validation_ticks;

    // ---- S2b: state builds through the verbs ---------------------------------
    // The mechanics rows below need state buildings on every major whatever the
    // handover left, so each major's champion BUILDS one through the legal verb
    // (apply_corp_command with `as_nation`) — the player's through the wire
    // seam — and the world runs live until they complete. A fixture made of
    // legal commands, never of hand-edited ownership.
    std::printf("--- S2b state builds through the verbs ---\n");
    {
        const char* const k_recipes[] = {"steel", "steel_bessemer", "refined_fuel", "coal_power_plant",
                                         "oil_power_plant", "ordnance", "propellant_atmospheric"};
        agent_protocol::session ps;
        ps.actor = w.player_entity;
        std::vector<entity_id> built;
        bool seam_built = false;
        for (const entity_id nid : sorted_nations(w))
        {
            if (!is_major_nation(w, nid))
                continue;
            const entity_id champ = champion_of(w, nid);
            // Fixture setup: a treasury the founding emptied could build
            // nothing; the rows below test the mechanics, not this seed's
            // purse, so each major is topped up to a working floor.
            w.nations.at(nid).treasury = std::max(w.nations.at(nid).treasury, 20000.0f);
            entity_id got = null_entity;
            int tries = 0;
            for (const entity_id tile : w.nations.at(nid).tiles)
            {
                if (got != null_entity || tries > 4000) break;
                for (const char* rn : k_recipes)
                {
                    const uint16_t rid = reg.recipe_id(rn);
                    if (rid == no_recipe || !state_runs(reg, building_type::processing_facility, rid))
                        continue;
                    ++tries;
                    if (nid == pn)
                    {
                        const std::string o = serve(w, reg, ps,
                            fmt("COMMAND corp=%u verb=0 tile=%u type=2 recipe=%u nation=%u",
                                static_cast<unsigned>(champ), static_cast<unsigned>(tile), rid,
                                static_cast<unsigned>(nid)));
                        const auto bpos = o.find("building=");
                        if (result_is(o, "applied") && bpos != std::string::npos)
                        {
                            got = static_cast<entity_id>(std::strtoul(o.c_str() + bpos + 9, nullptr, 10));
                            seam_built = true;
                        }
                    }
                    else
                    {
                        corp_command cmd;
                        cmd.tick = next_tick; cmd.corp = champ; cmd.verb = corp_verb::build;
                        cmd.type = building_type::processing_facility; cmd.tile = tile;
                        cmd.recipe = rid; cmd.as_nation = nid;
                        entity_id b = null_entity;
                        if (apply_corp_command(w, reg, cmd, &b) == corp_command_result::applied)
                            got = b;
                    }
                    if (got != null_entity) break;
                }
            }
            const bool owned = got != null_entity && owner_nation_of(w, got) == nid;
            std::printf("    major %u: state build %s (%d tries)%s\n", static_cast<unsigned>(nid),
                        owned ? "placed" : "NOT placed", tries,
                        owned ? fmt(" building %u, %d build tick(s)", static_cast<unsigned>(got),
                                    w.buildings.at(got).ticks_remaining).c_str() : "");
            if (owned) built.push_back(got);
        }
        check(!built.empty(), "S2b", fmt("champions placed state buildings through the verb (%zu)", built.size()));
        if (w.nations.count(pn))
            check(seam_built, "S2b", "the player's state build through the wire seam applies");
        bool nonneg = true;
        int ticks = 0;
        for (; ticks < 30; ++ticks)
        {
            bool all_done = true;
            for (const entity_id b : built)
                if (w.buildings.count(b) && w.buildings.at(b).ticks_remaining > 0) all_done = false;
            if (all_done) break;
            (void)run_settle_tick(w, reg, next_tick, 90 * (next_tick + 1), /*spectating=*/false);
            ++next_tick;
            for (const entity_id nid : state_nations(w))
                if (w.nations.at(nid).treasury < 0.0f) nonneg = false;
        }
        int done = 0;
        for (const entity_id b : built)
            if (w.buildings.count(b) && w.buildings.at(b).ticks_remaining == 0) ++done;
        for (const entity_id b : built)
            if (w.buildings.count(b))
            {
                const building_component& bc = w.buildings.at(b);
                std::printf("    site %u (%s): ticks_remaining %d progress %.2f paid %.1f state_idle %d owner %u treasury %.1f\n",
                            static_cast<unsigned>(b), reg.get_recipe(bc.recipe) ? reg.get_recipe(bc.recipe)->name.c_str() : "?",
                            bc.ticks_remaining, static_cast<double>(bc.construction_progress),
                            static_cast<double>(bc.construction_paid), bc.state_idle ? 1 : 0,
                            static_cast<unsigned>(owner_nation_of(w, b)),
                            static_cast<double>(w.nations.at(owner_nation_of(w, b)).treasury));
            }
        check(nonneg, "S2b", fmt("no state treasury below zero while they built (%d live ticks)", ticks));
        // Completion is the MARKET's to give (a site pauses when its materials
        // are short or over the fair-price ceiling, a corporation's alike) and
        // the TREASURY's (a site its treasury cannot fund waits): both are the
        // design, so the count is reported. What is checked is that the gate's
        // own record agrees with the buildings it idled.
        std::printf("    %d of %zu state builds completed in %d live ticks\n", done, built.size(), ticks);
        bool agree = true;
        for (const entity_id nid : state_nations(w))
        {
            int idle = 0;
            for (const entity_id b : w.nations.at(nid).state_assets)
                if (w.buildings.count(b) && w.buildings.at(b).state_idle) ++idle;
            const auto lit = w.state_ledger.find(nid);
            if (lit == w.state_ledger.end() || lit->second.idled != idle
                || lit->second.funded + lit->second.idled
                       != static_cast<int>(w.nations.at(nid).state_assets.size()))
                agree = false;
        }
        check(agree, "S2b", "the gate's ledger (funded + idled) agrees with every state building's state_idle");
    }

    // ---- S3: money through the treasury -----------------------------------
    std::printf("--- S3 money through the treasury ---\n");
    {
        world& t = w;
        const int tick = next_tick++;
        t.current_econ_tick = tick;
        lp_pool_map pools;
        advance_convoys(t);
        credit_arrived_convoys(t, 90);
        const auto T0 = treasuries(t);
        const std::size_t dec0 = t.ai_decisions.total;
        economy_report r = run_economy_step(t, reg, /*spectating=*/false, &pools);
        const auto T1 = treasuries(t);
        (void)run_trades(t, reg, r, &pools);
        const auto T2 = treasuries(t);
        auto flows = clear_markets(t, reg, r);
        const auto T3 = treasuries(t);
        apply_budget(t, reg, flows, r.workforce_contention, &r.budgets, &r.buildings, &r.building_labour);
        const auto T4 = treasuries(t);

        bool any_law = false;
        for (const law& l : t.laws) any_law |= l.enacted;
        // State retools this tick, priced at the switch cost (the scorer's
        // set_recipe is the one treasury-paid verb it issues).
        std::map<entity_id, int> retools;
        if (t.ai_decisions.total - dec0 <= corp_decision_ring::capacity)
            for (const corp_decision& d : t.ai_decisions.entries)
                if (d.tick == tick && d.command.as_nation != null_entity
                    && d.command.verb == corp_verb::set_recipe)
                    ++retools[d.command.as_nation];

        bool econ_ok = true, clear_ok = true, budget_ok = true, nonneg = true;
        float sold = 0.0f, bought = 0.0f;
        for (const entity_id nid : state_nations(t))
        {
            const world::state_tick_ledger& L = t.state_ledger.at(nid);
            const float switch_cost = static_cast<float>(retools[nid]) * reg.recipe_switch().switch_cost;
            if (!near(T1.at(nid) - T0.at(nid), -(L.construction + switch_cost), T0.at(nid)))
            {
                econ_ok = false;
                std::printf("    econ: nation %u moved %.4f, ledger says %.4f\n", static_cast<unsigned>(nid),
                            static_cast<double>(T1.at(nid) - T0.at(nid)),
                            static_cast<double>(-(L.construction + switch_cost)));
            }
            if (T3.at(nid) != T2.at(nid)) clear_ok = false;
            const float fi = flows.count(nid) ? flows.at(nid).income : 0.0f;
            const float fe = flows.count(nid) ? flows.at(nid).expenditure : 0.0f;
            const float net = L.income - L.expenditure - L.maintenance - L.wages;
            if (fi != L.income || fe != L.expenditure) budget_ok = false;
            // The state pass's own movement is exactly the ledger's net; what
            // apply_budget moved besides is the levy credits (transfers IN —
            // never negative), and with no law enacted there are none.
            const float state_move = L.treasury_close - L.treasury_settle_from;
            const float other_move = (T4.at(nid) - T3.at(nid)) - state_move;
            if (!near(state_move, net, L.treasury_settle_from) || other_move < -1e-2f
                || (!any_law && !near(other_move, 0.0f, T3.at(nid))) || L.treasury_close != T4.at(nid))
            {
                budget_ok = false;
                std::printf("    budget: nation %u state pass moved %.4f (net %.4f), other %.4f\n",
                            static_cast<unsigned>(nid), static_cast<double>(state_move),
                            static_cast<double>(net), static_cast<double>(other_move));
            }
            if (T4.at(nid) < 0.0f) nonneg = false;
            sold += L.income;
            bought += L.expenditure;
        }
        check(econ_ok, "S3", "the economy step moves a state treasury by exactly its construction and retools");
        check(clear_ok, "S3", "the clear moves no treasury (it pays through flows)");
        check(budget_ok, "S3", fmt("apply_budget's state pass moves a state treasury by exactly income - purchases "
                                   "- maintenance - wages; the rest is levy credit only (laws enacted: %s)",
                                   any_law ? "yes" : "no"));
        check(nonneg, "S3", "no state treasury below zero after settlement");
        std::printf("    state industry this tick: %.1f sold onto the shelf, %.1f bought off it\n",
                    static_cast<double>(sold), static_cast<double>(bought));
        run_nation_step(t, reg, r, tick);
        run_firm_exits(t, reg.firm_exit(), &r.firm_exits);
    }

    // ---- S4: an unpaid building idles ---------------------------------------
    std::printf("--- S4 an unpaid building idles ---\n");
    {
        entity_id victim = null_entity;
        std::size_t most = 0;
        for (const entity_id nid : state_nations(w))
            if (w.nations.at(nid).state_assets.size() > most)
            {
                most = w.nations.at(nid).state_assets.size();
                victim = nid;
            }
        if (victim == null_entity)
        {
            check(false, "S4", "no state nation to starve");
        }
        else
        {
            world c = w;
            c.nations.at(victim).treasury = 0.0f;
            const settle_tick_result r = run_settle_tick(c, reg, next_tick, 90 * (next_tick + 1),
                                                         /*spectating=*/false);
            const world::state_tick_ledger& L = c.state_ledger.at(victim);
            bool all_idle = true, none_ran = true;
            std::set<entity_id> sa(c.nations.at(victim).state_assets.begin(),
                                   c.nations.at(victim).state_assets.end());
            for (const entity_id bid : sa)
                if (!c.buildings.at(bid).state_idle) all_idle = false;
            for (const building_report& br : r.report.buildings)
                if (sa.count(br.building) && br.output_quantity > 0.0f) none_ran = false;
            check(all_idle && L.funded == 0 && L.idled == static_cast<int>(sa.size()), "S4",
                  fmt("a zero treasury idles all %zu of nation %u's state buildings", sa.size(),
                      static_cast<unsigned>(victim)));
            check(none_ran, "S4", "an idled state building produces nothing");
            check(L.expenditure == 0.0f && L.maintenance == 0.0f && L.wages == 0.0f
                      && L.construction == 0.0f && L.income == 0.0f,
                  "S4", "an idled state building pays nothing (no inputs, maintenance, wages or build)");
            check(c.nations.at(victim).treasury >= 0.0f, "S4", "the starved treasury is not below zero");

            c.nations.at(victim).treasury = 1.0e9f;
            (void)run_settle_tick(c, reg, next_tick + 1, 90 * (next_tick + 2), false);
            const world::state_tick_ledger& L2 = c.state_ledger.at(victim);
            check(L2.funded == static_cast<int>(sa.size()) && L2.idled == 0, "S4",
                  fmt("refunded, every one runs again (%d funded)", L2.funded));
        }
    }

    // ---- S5: the seam --------------------------------------------------------
    // ---- S8: changes after the gate cannot overdraw a treasury ---------------
    std::printf("--- S8 changes after the gate ---\n");
    if (w.nations.count(pn) && !w.nations.at(pn).state_assets.empty())
    {
        const entity_id champ = champion_of(w, pn);
        const auto running = [&](const world& c) {
            std::vector<entity_id> out;
            for (const entity_id b : c.nations.at(pn).state_assets)
                if (c.buildings.at(b).ticks_remaining == 0 && !c.buildings.at(b).decommissioned)
                    out.push_back(b);
            return out;
        };
        // (a) Commands: with the treasury exactly what the gate committed, a
        // raised target and a resume are refused (rejected_funds), nothing moved.
        {
            world c = w;
            std::vector<entity_id> run = running(c);
            entity_id parked = null_entity;
            if (run.size() >= 2) { parked = run.back(); c.buildings.at(parked).decommissioned = true; run.pop_back(); }
            for (const entity_id b : run) c.buildings.at(b).workforce_target = 50;
            c.nations.at(pn).treasury = 1.0e9f;
            plan_state_funding(c, reg);
            c.nations.at(pn).treasury = c.state_ledger.at(pn).committed;
            const std::uint64_t h0 = c.state_hash(0);
            corp_command cmd;
            cmd.corp = champ; cmd.as_nation = pn; cmd.verb = corp_verb::set_workforce;
            cmd.subject = run.empty() ? null_entity : run.front(); cmd.workforce = 200;
            const auto r1 = apply_corp_command(c, reg, cmd);
            corp_command_result r2 = corp_command_result::rejected_funds;
            if (parked != null_entity)
            {
                corp_command rs; rs.corp = champ; rs.as_nation = pn; rs.verb = corp_verb::resume; rs.subject = parked;
                r2 = apply_corp_command(c, reg, rs);
            }
            check(!run.empty() && r1 == corp_command_result::rejected_funds
                      && r2 == corp_command_result::rejected_funds && c.state_hash(0) == h0,
                  "S8a", "with nothing uncommitted, a raised workforce and a resume are rejected_funds, nothing moved");
        }
        // (b) The auto-solver: every running state building pinned low with
        // auto on, the treasury just covering that; a full live tick follows.
        {
            world c = w;
            const std::vector<entity_id> run = running(c);
            float need = 0.0f;
            for (const entity_id b : c.nations.at(pn).state_assets)
            {
                building_component& bc = c.buildings.at(b);
                if (bc.ticks_remaining == 0 && !bc.decommissioned) { bc.workforce_target = 10; bc.workforce_auto = true; }
                need += state_building_cost_bound(c, reg, bc) * 1.0006f;
            }
            c.nations.at(pn).treasury = need;
            int raised = 0;
            const settle_tick_result r = run_settle_tick(c, reg, next_tick, 90 * (next_tick + 1), false);
            (void)r;
            for (const entity_id b : run)
                if (c.buildings.at(b).workforce_target > 10) ++raised;
            check(c.state_ledger.at(pn).treasury_close >= 0.0f && c.nations.at(pn).treasury >= 0.0f, "S8b",
                  fmt("the auto-solver on a just-funded treasury leaves it >= 0 (%d of %zu raises funded, close %.2f)",
                      raised, run.size(), static_cast<double>(c.state_ledger.at(pn).treasury_close)));
        }
        // (c) A site finishing this tick runs from the next gate.
        {
            world c = w;
            const entity_id b = c.nations.at(pn).state_assets.front();
            building_component& bc = c.buildings.at(b);
            bc.ticks_remaining = 1; bc.construction_progress = 0.999f; bc.decommissioned = false;
            float need = 0.0f;
            for (const entity_id s : c.nations.at(pn).state_assets)
                need += state_building_cost_bound(c, reg, c.buildings.at(s)) * 1.0006f;
            c.nations.at(pn).treasury = need;
            const settle_tick_result r = run_settle_tick(c, reg, next_tick, 90 * (next_tick + 1), false);
            bool ran = false;
            for (const building_report& br : r.report.buildings)
                if (br.building == b && br.output_quantity > 0.0f) ran = true;
            const bool completed = c.buildings.at(b).ticks_remaining == 0;
            check(c.state_ledger.at(pn).treasury_close >= 0.0f && (!completed || (!ran && c.buildings.at(b).state_idle)),
                  "S8c", fmt("a state site completing this tick (%s) produces nothing until the next gate; treasury >= 0",
                             completed ? "completed" : "paused by its market"));
        }
        // (d) The scorer on tight treasuries: every state nation funded to the
        // penny, a spectated tick (every champion dials, the player's too).
        {
            world c = w;
            for (const entity_id nid : state_nations(c))
            {
                float need = 0.0f;
                for (const entity_id s : c.nations.at(nid).state_assets)
                {
                    building_component& bc = c.buildings.at(s);
                    bc.ai_cooldown = 0;
                    need += state_building_cost_bound(c, reg, bc) * 1.0006f;
                }
                c.nations.at(nid).treasury = need;
            }
            bool nonneg = true;
            for (int k = 0; k < 4; ++k)
            {
                (void)run_settle_tick(c, reg, next_tick + k, 90 * (next_tick + k + 1), /*spectating=*/true);
                for (const entity_id nid : state_nations(c))
                    if (c.state_ledger.count(nid) && c.state_ledger.at(nid).treasury_close < 0.0f) nonneg = false;
            }
            check(nonneg, "S8d", "four spectated ticks on to-the-penny treasuries: no state pass ends below zero");
        }
    }
    else
        check(false, "S8", "the player's nation holds no state building (the founding should guarantee one)");

    std::printf("--- S5 the seam ---\n");

    {
        agent_protocol::session s;
        s.actor = w.player_entity;
        const entity_id P = w.player_entity;
        entity_id own_sb = null_entity;
        if (w.nations.count(pn) && !w.nations.at(pn).state_assets.empty())
            own_sb = w.nations.at(pn).state_assets.front();
        entity_id other_n = null_entity, other_sb = null_entity;
        for (const entity_id nid : state_nations(w))
            if (nid != pn) { other_n = nid; other_sb = w.nations.at(nid).state_assets.front(); break; }
        entity_id own_tile = null_entity;
        if (w.nations.count(pn))
            for (const entity_id t : w.nations.at(pn).tiles)
                if (w.tiles.count(t) && !placement_rules::is_water_tile(w.tiles.at(t).substrate))
                { own_tile = t; break; }
        const uint16_t steel = reg.recipe_id("steel");
        const uint16_t food  = reg.recipe_id("food_rations");
        const auto P_ = [&](const std::string& rest) {
            return fmt("COMMAND corp=%u ", static_cast<unsigned>(P)) + rest;
        };
        struct rej_row { std::string line; const char* want; const char* what; };
        std::vector<rej_row> rows;
        if (other_n != null_entity)
            rows.push_back({P_(fmt("verb=3 subject=%u workforce=50 nation=%u", other_sb, other_n)),
                            "rejected_not_owner", "a state command for ANOTHER nation (actor gate)"});
        if (other_sb != null_entity && pn != null_entity)
            rows.push_back({P_(fmt("verb=3 subject=%u workforce=50 nation=%u", other_sb, pn)),
                            "rejected_not_owner", "own nation, a building it does not own (seam)"});
        rows.push_back({P_(fmt("verb=7 subject=%u nation=%u", w.home_body, pn)),
                        "rejected_invalid", "a non-building verb (survey) for the state"});
        rows.push_back({P_("verb=3 subject=1 workforce=50 nation=abc"),
                        "rejected_invalid", "a malformed nation id (parse)"});
        rows.push_back({P_(fmt("verb=3 subject=1 workforce=50 nation=%llu", 1ull << 40)),
                        "rejected_invalid", "a nation id past the entity domain (parse)"});
        if (own_sb != null_entity)
            rows.push_back({P_(fmt("verb=3 subject=%u workforce=201 nation=%u", own_sb, pn)),
                            "rejected_invalid", "workforce 201 on its own state building (parse)"});
        if (own_tile != null_entity)
            rows.push_back({P_(fmt("verb=0 tile=%u type=2 recipe=%u nation=%u", own_tile, food, pn)),
                            "rejected_invalid", "a state build on a private-sector recipe"});
        if (own_tile != null_entity)
            rows.push_back({P_(fmt("verb=0 tile=%u type=1 nation=%u", own_tile, pn)),
                            "rejected_invalid", "a state build of an extraction site"});
        {
            entity_id rival = null_entity;
            for (const auto& [cid, cc] : w.corporations)
                if (!cc.is_background && cid != P) { rival = cid; break; }
            if (rival != null_entity)
                rows.push_back({fmt("COMMAND corp=%u verb=3 subject=%u workforce=50 nation=%u",
                                    static_cast<unsigned>(rival), other_sb, other_n),
                                "rejected_not_owner", "acting as a rival champion (actor gate)"});
        }
        for (const rej_row& row : rows)
        {
            const std::uint64_t h0 = w.state_hash(0);
            const std::string o = serve(w, reg, s, row.line);
            const bool ok = result_is(o, row.want) && w.state_hash(0) == h0;
            check(ok, "S5", std::string(row.what) + " -> " + row.want + ", nothing mutated"
                                + (ok ? std::string() : "  [got: " + o.substr(0, o.size() - 1) + "]"));
        }
        // A state build the treasury cannot cover: rejected_funds, nothing mutated.
        if (own_tile != null_entity && steel != no_recipe)
        {
            const float keep = w.nations.at(pn).treasury;
            w.nations.at(pn).treasury = 0.0f;
            const std::uint64_t h0 = w.state_hash(0);
            const std::string o = serve(w, reg, s, P_(fmt("verb=0 tile=%u type=2 recipe=%u nation=%u",
                                                          own_tile, steel, pn)));
            const bool ok = (result_is(o, "rejected_funds") || result_is(o, "rejected_placement")
                             || result_is(o, "rejected_wrong_air") || result_is(o, "rejected_tech_locked"))
                         && w.state_hash(0) == h0;
            check(ok, "S5", "a state build with an empty treasury is refused, nothing mutated  [" +
                                o.substr(0, o.size() - 1) + "]");
            w.nations.at(pn).treasury = keep;
        }
        if (own_sb != null_entity)
        {
            const std::string o = serve(w, reg, s, P_(fmt("verb=3 subject=%u workforce=80 nation=%u",
                                                          own_sb, pn)));
            const building_component& b = w.buildings.at(own_sb);
            check(result_is(o, "applied") && b.workforce_target == 80 && !b.workforce_auto, "S5",
                  "set_workforce on its own state building applies (the dial pins)");
            const std::string o2 = serve(w, reg, s, P_(fmt("verb=11 subject=%u nation=%u", own_sb, pn)));
            check(result_is(o2, "applied") && w.buildings.at(own_sb).workforce_auto, "S5",
                  "set_workforce_auto hands the dial back");
        }
        else
            std::printf("    (the player's nation holds no state building on this seed: own-building rows skipped)\n");
    }

    // ---- S6: the grant's scope -------------------------------------------------
    std::printf("--- S6 the grant's scope ---\n");
    {
        // The cooldown is SNAPSHOTTED, not required zero: a building the
        // settle's spectated scorer touched before the seat keeps that hold,
        // and only an evaluation of its owner ticks it down — the player is
        // never evaluated, so it simply stands. A scorer touch would reset it
        // upward; any change at all fails the row.
        struct snap { uint16_t recipe; bool decom; int cooldown; };
        std::map<entity_id, snap> mine;
        if (w.nations.count(pn))
            for (const entity_id bid : w.nations.at(pn).state_assets)
                mine[bid] = {w.buildings.at(bid).recipe, w.buildings.at(bid).decommissioned,
                             w.buildings.at(bid).ai_cooldown};
        bool untouched = true, no_decision = true;
        int rival_state_decisions = 0;
        std::set<entity_id> rival_touched; // rival state buildings the scorer put on hold
        for (int k = 0; k < 8; ++k)
        {
            const int tick = next_tick++;

            const std::size_t dec0 = w.ai_decisions.total;
            (void)run_settle_tick(w, reg, tick, 90 * (tick + 1), /*spectating=*/false);
            for (const auto& [bid, sn] : mine)
            {
                const auto bit = w.buildings.find(bid);
                if (bit == w.buildings.end()) { untouched = false; continue; }
                if (bit->second.recipe != sn.recipe || bit->second.decommissioned != sn.decom
                    || bit->second.ai_cooldown != sn.cooldown)
                {
                    if (untouched)
                        std::printf("    touched: building %u recipe %u->%u decom %d->%d cooldown %d->%d\n",
                                    static_cast<unsigned>(bid), sn.recipe, bit->second.recipe, sn.decom,
                                    bit->second.decommissioned, sn.cooldown, bit->second.ai_cooldown);
                    untouched = false;
                }
            }
            const std::size_t n_new = std::min<std::size_t>(w.ai_decisions.total - dec0,
                                                            w.ai_decisions.entries.size());
            for (const entity_id nid : state_nations(w))
                if (nid != pn)
                    for (const entity_id bid : w.nations.at(nid).state_assets)
                        if (w.buildings.count(bid) && w.buildings.at(bid).ai_cooldown > 0)
                            rival_touched.insert(bid);
            for (const corp_decision& d : w.ai_decisions.entries)
                if (d.tick == tick)
                {
                    if (d.command.as_nation == pn && pn != null_entity) no_decision = false;
                    if (d.command.as_nation != null_entity) ++rival_state_decisions;
                }
            (void)n_new;
        }
        check(untouched, "S6", fmt("the player's %zu state buildings: no recipe, idle or cooldown change by "
                                   "any AI tier over 8 live ticks", mine.size()));
        check(no_decision, "S6", "no logged decision names the player's nation");
        std::printf("    rival state industry under the champions' layer: %d decision(s) logged, %zu "
                    "building(s) put on a scorer hold\n", rival_state_decisions, rival_touched.size());

    }

    // ---- S7: the save ------------------------------------------------------
    std::printf("--- S7 the save ---\n");
    {
        // Idle one state building so the idle list is exercised too.
        entity_id idled = null_entity;
        for (const entity_id nid : state_nations(w))
        {
            idled = w.nations.at(nid).state_assets.back();
            w.buildings.at(idled).state_idle = true;
            break;
        }
        const std::string bytes = to_bytes(w);
        world back;
        std::istringstream in(bytes, std::ios::binary);
        const bool ok = read_world_snapshot(back, in);
        bool same = ok;
        for (const auto& [nid, nc] : w.nations)
            same = same && back.nations.count(nid) && back.nations.at(nid).state_assets == nc.state_assets;
        for (const auto& [bid, b] : w.buildings)
            same = same && back.buildings.count(bid) && back.buildings.at(bid).state_idle == b.state_idle;
        check(same && back.state_hash(0) == w.state_hash(0) && to_bytes(back) == bytes, "S7",
              "v48 round-trips state_assets and state_idle, state_hash equal, bytes equal");

        // The v47 stream: the same bytes less the trailing state section.
        std::size_t tail = sizeof(uint32_t);
        for (const entity_id nid : state_nations(w))
            tail += sizeof(entity_id) + sizeof(uint32_t)
                  + w.nations.at(nid).state_assets.size() * sizeof(entity_id);
        std::size_t idle_n = 0;
        for (const auto& [bid, b] : w.buildings) idle_n += b.state_idle ? 1 : 0;
        tail += sizeof(uint32_t) + idle_n * sizeof(entity_id);
        std::string v47 = bytes.substr(0, bytes.size() - tail);
        const uint32_t ver = k_world_save_version_pre_state;
        std::memcpy(&v47[4], &ver, sizeof ver);
        world old;
        std::istringstream in47(v47, std::ios::binary);
        const bool ok47 = read_world_snapshot(old, in47);
        bool none = ok47;
        for (const auto& [nid, nc] : old.nations) none = none && nc.state_assets.empty();
        for (const auto& [bid, b] : old.buildings) none = none && !b.state_idle;
        check(none && old.buildings.size() == w.buildings.size(), "S7",
              "a v47 stream reads, as a world with no state industry");
        if (idled != null_entity) w.buildings.at(idled).state_idle = false;
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--strict") == 0) { g_strict = true; continue; }
        seeds.push_back(static_cast<std::uint32_t>(std::strtoul(argv[i], nullptr, 10)));
    }
    if (seeds.empty())
        seeds.push_back(0);

    lua_state lua;
    for (const std::uint32_t s : seeds)
        run_seed(lua, s);

    std::printf("\nstate_industry_harness: %d pass, %d fail, %d finding(s) -- %s\n", g_pass, g_fail,
                g_finding, g_fail == 0 ? "ALL PASS" : "FAILURES");
    return g_fail == 0 ? 0 : 1;
}
