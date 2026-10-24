// ---------------------------------------------------------------------------
// fold_census — BL-1402 (fold to ground), requirement group `fold-to-ground`
// ---------------------------------------------------------------------------
// docs/politics/NATIONS.md § What a nation owns / § Own ground (Ben, 2026-10-23).
// On the SHIPPED start (harness_params.hpp's app mirror, the fold in its shipped
// place), per seed, reading the fold through the mirror's `fold_probe`:
//
//   F1  THE FOLD:
//         F1a no corporation owns a building, and every nation-owned building
//             stands on its owner's ground; nothing is listed twice;
//         F1b every building a corporation held either joined a nation or was
//             demolished on unowned ground, as the report counts them;
//         F1c no unit, standing trade, convoy, route or track tile is a
//             corporation's, and every unit with a muster base belongs to the
//             base's owner.  (R1)
//   F2  MONEY, EXACTLY. Each nation's treasury right after the fold equals its
//       treasury right before plus every positive balance of a corporation
//       homed there, added in float in ascending corporation id - the fold's
//       own order. A negative balance is forgiven (counted), never carried.
//       No treasury is negative.  (R2)
//   F3  IDEMPOTENT. A second fold on the started world moves nothing (state_hash
//       equal, nothing folded, re-homed or demolished).  (R1)
//   F4  THE SAVE. The folded start writes and reads back with an equal
//       state_hash.  (R3)
//
// Build: cmd //c tools\verify\build_lua_harness.bat fold_census
// Run:   build_gen/verify/fold_census.exe [seed ...]   (default: 0)
// Exits non-zero on any FAIL.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/state_industry.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* row, const std::string& what)
{
    std::printf("  [%s] %s %s\n", ok ? "PASS" : "FAIL", row, what.c_str());
    if (ok) ++g_pass; else ++g_fail;
}

std::string fmt(const char* f, ...)
{
    char buf[640];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

bool no_firm_holdings(const world& w, std::string& why)
{
    for (const auto& [cid, cc] : w.corporations)
        if (!cc.assets.empty())
        {
            why = fmt("corporation %llu still holds %zu building(s)",
                      static_cast<unsigned long long>(cid), cc.assets.size());
            return false;
        }
    std::set<entity_id> seen;
    for (const auto& [nid, nc] : w.nations)
        for (const entity_id bid : nc.state_assets)
        {
            if (!seen.insert(bid).second)
            {
                why = fmt("building %llu listed twice", static_cast<unsigned long long>(bid));
                return false;
            }
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
            {
                why = fmt("nation %llu lists a dangling building %llu",
                          static_cast<unsigned long long>(nid), static_cast<unsigned long long>(bid));
                return false;
            }
            const auto tn = w.tile_to_nation.find(bit->second.tile);
            if (tn == w.tile_to_nation.end() || tn->second != nid)
            {
                why = fmt("building %llu of nation %llu stands off its ground",
                          static_cast<unsigned long long>(bid), static_cast<unsigned long long>(nid));
                return false;
            }
        }
    return true;
}

void run_seed(lua_state& lua, std::uint32_t seed)
{
    std::printf("\n=== seed %u ===\n", static_cast<unsigned>(seed));
    world_params params = arc_params(world_arc::shipped);
    params.seed = seed;

    app_start_world out;
    fold_probe probe;
    harness_charter_input ci;
    ci.probe = &probe;
    build_app_start_world(lua, params, out, ci);
    world& w = out.w;
    const fold_report& r = probe.report;
    check(probe.ran, "F0", "the shipped mirror ran the fold");
    if (!probe.ran)
        return;

    // ---- F1 -----------------------------------------------------------------
    std::string why;
    const bool clean = no_firm_holdings(w, why);
    check(clean, "F1a", clean ? fmt("no corporation owns a building; %d folded, %d re-homed, all on their "
                                    "owner's ground", r.buildings_folded, r.rehomed)
                              : why);
    {
        int joined = 0, gone = 0;
        for (const entity_id bid : probe.held_before)
        {
            if (w.buildings.find(bid) == w.buildings.end()) ++gone;
            else if (owner_nation_of(w, bid) != null_entity) ++joined;
        }
        const int lost = static_cast<int>(probe.held_before.size()) - joined - gone;
        check(lost == 0 && gone == r.firm_on_unowned && joined == r.firm_folded, "F1b",
              fmt("of %zu firm buildings: %d joined a nation (report %d), %d demolished on unowned ground "
                  "(report %d), %d lost", probe.held_before.size(), joined, r.firm_folded, gone,
                  r.firm_on_unowned, lost));
    }
    {
        int firm_units = 0, firm_trades = 0, firm_convoys = 0, firm_routes = 0, firm_track = 0, off_base = 0;
        for (const auto& [uid, u] : w.units)
        {
            if (w.corporations.count(u.owner)) ++firm_units;
            if (u.muster_base != null_entity && w.buildings.count(u.muster_base)
                && owner_nation_of(w, u.muster_base) != u.owner)
                ++off_base;
        }
        for (const auto& t : w.trades) if (w.corporations.count(t.owner)) ++firm_trades;
        for (const auto& c : w.convoys) if (w.corporations.count(c.corp)) ++firm_convoys;
        for (const auto& rt : w.trade_routes) if (w.corporations.count(rt.corp)) ++firm_routes;
        for (const auto& [tile, owner] : w.rail_track) if (w.corporations.count(owner)) ++firm_track;
        check(firm_units + firm_trades + firm_convoys + firm_routes + firm_track + off_base == 0, "F1c",
              fmt("nothing else is a corporation's: units %d, trades %d, convoys %d, routes %d, track %d; "
                  "units off their base's owner %d (re-owned: units %d, trades %d, convoys %d, routes %d, "
                  "track %d; routes merged %d)", firm_units, firm_trades, firm_convoys, firm_routes,
                  firm_track, off_base, r.units_reowned, r.trades_reowned, r.convoys_reowned,
                  r.routes_reowned, r.rail_reowned, r.routes_merged));
    }

    // ---- F2: money, recomputed in the fold's own float order ----------------
    {
        std::map<entity_id, float> expect;
        for (const auto& [nid, t] : probe.treasury_before) expect[nid] = t;
        float forgiven = 0.0f, stranded = 0.0f;
        for (const auto& [cid, home, bal] : probe.balances) // ascending corp id, as the fold walks
        {
            if (bal > 0.0f)
            {
                const auto e = expect.find(home);
                if (e != expect.end()) e->second += bal;
                else stranded += bal;
            }
            else if (bal < 0.0f)
                forgiven -= bal;
        }
        int mismatched = 0, negative = 0;
        for (const auto& [nid, t] : probe.treasury_after)
        {
            if (expect.count(nid) == 0 || expect.at(nid) != t) ++mismatched;
            if (t < 0.0f) ++negative;
        }
        check(mismatched == 0 && negative == 0 && forgiven == r.debt_forgiven
                  && stranded == r.balance_stranded, "F2",
              fmt("every treasury after the fold is exactly its own before plus its firms' positive "
                  "balances (%d mismatched, %d negative); %.3f moved, %.3f forgiven (report %.3f), %.3f "
                  "stranded (report %.3f), %.3f reserve dropped", mismatched, negative,
                  static_cast<double>(r.balance_moved), static_cast<double>(forgiven),
                  static_cast<double>(r.debt_forgiven), static_cast<double>(stranded),
                  static_cast<double>(r.balance_stranded), static_cast<double>(r.reserve_dropped)));
    }

    // ---- F3: idempotent -------------------------------------------------------
    {
        const std::uint64_t h0 = w.state_hash(0);
        const fold_report again = fold_to_ground(w);
        check(w.state_hash(0) == h0 && again.buildings_folded == 0 && again.rehomed == 0
                  && again.on_unowned == 0, "F3",
              fmt("a second fold moves nothing (folded %d, re-homed %d, demolished %d, hash %s)",
                  again.buildings_folded, again.rehomed, again.on_unowned,
                  w.state_hash(0) == h0 ? "equal" : "MOVED"));
    }

    // ---- F4: the save ---------------------------------------------------------
    {
        std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
        write_world_snapshot(w, ss);
        world back;
        ss.seekg(0);
        const bool read = read_world_snapshot(back, ss);
        check(read && back.state_hash(0) == w.state_hash(0), "F4",
              fmt("the folded start round-trips the save (read %d, hash %s)", read ? 1 : 0,
                  read && back.state_hash(0) == w.state_hash(0) ? "equal" : "DIFFERS"));
    }
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    for (int i = 1; i < argc; ++i)
        seeds.push_back(static_cast<std::uint32_t>(std::strtoul(argv[i], nullptr, 10)));
    if (seeds.empty())
        seeds.push_back(0);

    lua_state lua;
    for (const std::uint32_t s : seeds)
        run_seed(lua, s);

    std::printf("\nfold_census: %d pass, %d fail -- %s\n", g_pass, g_fail,
                g_fail == 0 ? "ALL PASS" : "FAILURES");
    return g_fail == 0 ? 0 : 1;
}
