// Opening fleet harness (BL-1532, opening fleets; no SDL / ImGui).
//
// The kit fleet is laid before the settle (finish_campaign_world; the harness mirror in
// harness_params.hpp). This harness builds the real app start world on each seed and
// checks MILITARY.md § Sea, "A major's opening fleet is fixed", against the active kit:
//
//   F1  A major with a sea-mouthed port of its own opens with exactly the kit's fleet,
//       class for class, every ship raised (muster 0) on the nation's live design and
//       lying at its home port's mouth; a major with none opens with no ship.
//   F2  Every major's sea pool opens at the kit's figure.
//   F3  No minor opens with a ship.
//   F4  The berths: the major's sea-mouthed ports ordered capital province first, then
//       straight-line distance from the capital, then tile and id; the kit's ships in
//       creation order take `per_port` at each in turn, wrapping. Re-derived here.
//   F5  No major whose ground keeps a sea-mouthed port... is checked by F1's count; and
//       the cap rule: a major holding ONLY lake ports is reported, never asserted (its
//       ground may hold no sea coast at all).
//   S1  Every opening ship survives the settle; ships whose HP fell are counted.
//   S2  A save round-trip keeps every ship, field for field, and the state hash.
//   D1  (first seed only) Two builds of the seed hash alike and lay the same ships.
//
// The process exits non-zero if any assertion FAILs.
//
// Usage (repo root): build_gen/verify/opening_fleet_settle.exe [--seeds a,b]
// Build: bash tools/verify/build_lua_harness.sh opening_fleet_settle
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/air_combat.hpp"   // air_tile_distance
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/recipe_registry.hpp"
#include "world/sea_combat.hpp"   // port_mouth_tile
#include "world/unit_designs.hpp" // ship_design_of
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace {

int g_fail = 0;

void check(bool ok, const std::string& what, std::uint32_t seed)
{
    if (!ok || what.rfind("S", 0) == 0 || what.rfind("D", 0) == 0)
        std::printf("  %s %s (seed %u)\n", ok ? "PASS" : "FAIL", what.c_str(), seed);
    if (!ok)
        ++g_fail;
}

/// The major's own completed, standing, sea-mouthed ports in berthing order.
std::vector<entity_id> berths_of(const world& w, entity_id nid)
{
    const nation_component& nc = w.nations.at(nid);
    const uint32_t cap = w.provinces.province_of(nc.capital_tile);
    std::vector<std::tuple<int, int, entity_id, entity_id>> rows;
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.type != building_type::port || b.ticks_remaining > 0 || b.decommissioned)
            continue;
        const auto tn = w.tile_to_nation.find(b.tile);
        if (tn == w.tile_to_nation.end() || tn->second != nid || port_mouth_tile(w, bid) == null_entity)
            continue;
        const bool at_cap = cap != 0 && w.provinces.province_of(b.tile) == cap;
        const int d = air_tile_distance(w, nc.capital_tile, b.tile);
        rows.emplace_back(at_cap ? 0 : 1, d < 0 ? std::numeric_limits<int>::max() : d, b.tile, bid);
    }
    std::sort(rows.begin(), rows.end());
    std::vector<entity_id> out;
    for (const auto& r : rows)
        out.push_back(std::get<3>(r));
    return out;
}

struct seed_out
{
    std::uint64_t hash = 0;
    std::vector<std::pair<entity_id, sea_ship>> ships;
};

seed_out run_seed(std::uint32_t seed, bool quiet)
{
    seed_out so;
    lua_state lua;
    world_params p;
    p.seed = seed;
    auto start = std::make_unique<app_start_world>();
    try { build_app_start_world(lua, p, *start); }
    catch (const std::exception& e) { std::printf("seed %u: build threw: %s\n", seed, e.what()); ++g_fail; return so; }
    world& w = start->w;
    const recipe_registry& reg = start->reg;
    so.hash = w.state_hash(0);
    for (const auto& [sid, s] : w.ships)
        so.ships.emplace_back(sid, s);
    if (quiet)
        return so;

    const opening_kit_config* kit = start->cfg.opening_kit.active_config();
    check(kit != nullptr && kit->has_fleet(), "F0 the active kit authors a fleet", seed);
    if (kit == nullptr || !kit->has_fleet())
        return so;

    std::map<entity_id, std::vector<std::pair<entity_id, const sea_ship*>>> by_owner;
    for (const auto& [sid, s] : w.ships)
        by_owner[s.owner].emplace_back(sid, &s); // ascending id: creation order
    int majors = 0, fleets = 0, lake_only = 0, laid = 0;
    for (const auto& [nid, nc] : w.nations)
    {
        const auto& mine = by_owner[nid];
        const std::string who = " (nation " + std::to_string(static_cast<unsigned long long>(nid)) + ")";
        if (!nc.is_major)
        {
            check(mine.empty(), "F3 a minor opens with no ship" + who, seed);
            continue;
        }
        ++majors;
        check(nc.sea_recruits == kit->sea_recruits, "F2 the major's sea pool opens at the kit's figure" + who, seed);
        const std::vector<entity_id> berths = berths_of(w, nid);
        if (berths.empty())
        {
            check(mine.empty(), "F1 a major with no sea port opens with no ship" + who, seed);
            bool any_port = false;
            for (const auto& [bid, b] : w.buildings)
                if (b.type == building_type::port && !b.decommissioned)
                    if (const auto tn = w.tile_to_nation.find(b.tile); tn != w.tile_to_nation.end() && tn->second == nid)
                        any_port = true;
            lake_only += any_port ? 1 : 0;
            continue;
        }
        ++fleets;
        check(mine.size() == kit->fleet.size(), "F1 the major opens with the kit's ship count" + who, seed);
        if (mine.size() != kit->fleet.size())
            continue;
        for (std::size_t i = 0; i < mine.size(); ++i)
        {
            const sea_ship& s = *mine[i].second;
            ++laid;
            check(s.type == kit->fleet[i], "F1 ship class in the kit's order" + who, seed);
            check(s.muster_days == 0 && !s.refitting, "F1 ship stands raised" + who, seed);
            check(s.kit == ship_design_of(w, nid, static_cast<ship_class>(s.type)), "F1 ship on the live design" + who, seed);
            const entity_id want = berths[(i / static_cast<std::size_t>(std::max(1, kit->fleet_per_port))) % berths.size()];
            check(s.home_port == want, "F4 ship berthed in the berthing order" + who, seed);
            check(s.tile == port_mouth_tile(w, s.home_port), "F1 ship lies at its home port's mouth" + who, seed);
        }
    }
    std::printf("seed %u: %d major(s), %d with a fleet, %d ship(s); %d major(s) holding only lake ports\n", seed,
                majors, fleets, laid, lake_only);

    const std::size_t before = w.ships.size();
    std::map<entity_id, int32_t> hp0;
    for (const auto& [sid, s] : w.ships)
        hp0[sid] = s.hp_ppm;
    run_app_validation_settle(w, reg);
    int hurt = 0;
    for (const auto& [sid, s] : w.ships)
        if (const auto it = hp0.find(sid); it != hp0.end() && s.hp_ppm < it->second)
            ++hurt;
    std::printf("seed %u: %zu ship(s) before the settle, %zu after; %d lost HP in it\n", seed, before,
                w.ships.size(), hurt);
    bool kept = w.ships.size() == before;
    for (const auto& [sid, h] : hp0)
        kept = kept && w.ships.count(sid) != 0;
    check(kept, "S1 every opening ship survives the settle", seed);

    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
    write_world_snapshot(w, ss);
    world back;
    const bool read = read_world_snapshot(back, ss, &reg.military());
    check(read, "S2a the snapshot reads back", seed);
    if (!read)
        return so;
    bool same = back.ships.size() == w.ships.size();
    for (auto a = w.ships.begin(), b = back.ships.begin(); same && a != w.ships.end(); ++a, ++b)
    {
        const sea_ship& x = a->second;
        const sea_ship& y = b->second;
        same = a->first == b->first && x.owner == y.owner && x.home_port == y.home_port && x.tile == y.tile
            && x.type == y.type && x.hp_ppm == y.hp_ppm && x.muster_days == y.muster_days
            && x.supply_factor_permille == y.supply_factor_permille && x.kit == y.kit;
    }
    check(same, "S2b every ship reads back field for field", seed);
    check(back.state_hash(0) == w.state_hash(0), "S2c the state hash survives the round-trip", seed);
    return so;
}

std::vector<std::uint32_t> parse_seed_list(const std::string& s)
{
    std::vector<std::uint32_t> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ','))
        if (!tok.empty())
            out.push_back(static_cast<std::uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
    return out;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds{0};
    bool det = true;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) seeds = parse_seed_list(argv[++i]);
        else if (!std::strcmp(argv[i], "--no-determinism")) det = false;
        else { std::fprintf(stderr, "usage: opening_fleet_settle [--seeds a,b] [--no-determinism]\n"); return 2; }
    }
    for (std::size_t k = 0; k < seeds.size(); ++k)
    {
        const seed_out a = run_seed(seeds[k], /*quiet=*/false);
        if (k == 0 && det)
        {
            const seed_out b = run_seed(seeds[k], /*quiet=*/true);
            bool same = a.hash == b.hash && a.ships.size() == b.ships.size();
            for (std::size_t i = 0; same && i < a.ships.size(); ++i)
                same = a.ships[i].first == b.ships[i].first && a.ships[i].second.home_port == b.ships[i].second.home_port
                    && a.ships[i].second.type == b.ships[i].second.type;
            check(same, "D1 two builds of the seed hash alike and lay the same ships", seeds[k]);
        }
    }
    std::printf("opening_fleet_settle: %s (%d failure(s))\n", g_fail == 0 ? "PASS" : "FAIL", g_fail);
    return g_fail == 0 ? 0 : 1;
}
