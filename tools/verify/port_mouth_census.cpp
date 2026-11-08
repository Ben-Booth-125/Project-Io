// Port mouth census (BL-1532, opening fleets; no SDL / ImGui).
//
// Before a nation's opening fleet can be berthed, every nation must hold a port a ship
// can lie at: a completed, standing port with SEA within two steps (`port_mouth_tile`).
// NR-1149 (5) saw the --verify world's seat open with a port on a lake. This census
// builds the real app start world on each library seed and reads, per nation, how many
// standing ports it holds, how many have no sea mouth, and whether its capital province
// holds one with a mouth. It asserts nothing: it is the measurement that says whether
// generation ever opens a lake port, and how often a major has no sea port at all.
//
// Usage (repo root): build_gen/verify/port_mouth_census.exe [--seeds a,b]
// Build: bash tools/verify/build_lua_harness.sh port_mouth_census
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/recipe_registry.hpp"
#include "world/sea_combat.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<std::uint32_t> library_seeds(const char* path)
{
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    const std::string text = ss.str();
    std::vector<std::uint32_t> out;
    std::size_t pos = text.find("\"seeds\"");
    const std::string key = "\"seed\"";
    while (pos != std::string::npos && (pos = text.find(key, pos)) != std::string::npos)
    {
        pos = text.find(':', pos);
        if (pos == std::string::npos)
            break;
        out.push_back(static_cast<std::uint32_t>(std::strtoul(text.c_str() + pos + 1, nullptr, 10)));
    }
    return out;
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

struct tally
{
    int nations = 0, majors = 0;
    int ports = 0, lake_ports = 0;
    int no_port = 0, no_sea_port = 0, majors_no_sea_port = 0;
    int capital_sea_port = 0, majors_capital_sea_port = 0;
};

void run_seed(std::uint32_t seed, tally& all)
{
    lua_state lua;
    world_params p;
    p.seed = seed;
    auto start = std::make_unique<app_start_world>();
    try { build_app_start_world(lua, p, *start); }
    catch (const std::exception& e) { std::printf("seed %u: build threw: %s\n", seed, e.what()); return; }
    world& w = start->w;

    struct row { int ports = 0, lake = 0; bool capital_sea = false; };
    std::map<entity_id, row> rows;
    for (const auto& [nid, nc] : w.nations)
        rows[nid];
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.type != building_type::port || b.ticks_remaining > 0 || b.decommissioned)
            continue;
        const auto tn = w.tile_to_nation.find(b.tile);
        if (tn == w.tile_to_nation.end() || rows.count(tn->second) == 0)
            continue;
        row& r = rows[tn->second];
        ++r.ports;
        const bool mouth = port_mouth_tile(w, bid) != null_entity;
        if (!mouth)
            ++r.lake;
        const nation_component& nc = w.nations.at(tn->second);
        const uint32_t pc = w.provinces.province_of(b.tile);
        const uint32_t cc = w.provinces.province_of(nc.capital_tile);
        if (mouth && pc != 0 && pc == cc)
            r.capital_sea = true;
        if (nc.is_major)
            std::printf("  seed %u major %llu port %llu: tile %llu%s, %s, capital province %s\n", seed,
                        static_cast<unsigned long long>(tn->second), static_cast<unsigned long long>(bid),
                        static_cast<unsigned long long>(b.tile), b.tile == nc.capital_tile ? " (the capital tile)" : "",
                        mouth ? "sea mouth" : "NO SEA MOUTH", pc != 0 && pc == cc ? "yes" : "no");
    }
    std::map<entity_id, int> ships;
    for (const auto& [sid, sh] : w.ships)
    {
        ++ships[sh.owner];
        std::printf("  seed %u ship %llu: owner %llu row %u home port %llu at mouth %llu, muster %d\n", seed,
                    static_cast<unsigned long long>(sid), static_cast<unsigned long long>(sh.owner),
                    static_cast<unsigned>(sh.type), static_cast<unsigned long long>(sh.home_port),
                    static_cast<unsigned long long>(sh.tile), static_cast<int>(sh.muster_days));
    }
    std::printf("HASH %u: %016llx\n", seed, static_cast<unsigned long long>(w.state_hash(0)));
    tally t;
    for (const auto& [nid, r] : rows)
    {
        const bool major = w.nations.at(nid).is_major;
        ++t.nations;
        t.majors += major ? 1 : 0;
        t.ports += r.ports;
        t.lake_ports += r.lake;
        t.no_port += r.ports == 0 ? 1 : 0;
        const bool sea = r.ports - r.lake > 0;
        t.no_sea_port += sea ? 0 : 1;
        t.majors_no_sea_port += (major && !sea) ? 1 : 0;
        t.capital_sea_port += r.capital_sea ? 1 : 0;
        t.majors_capital_sea_port += (major && r.capital_sea) ? 1 : 0;
        if (major)
            std::printf("  seed %u major %llu: ports %d, lake %d, capital-province sea port %s, ships %d, sea pool %d\n",
                        seed, static_cast<unsigned long long>(nid), r.ports, r.lake, r.capital_sea ? "yes" : "no",
                        ships[nid], w.nations.at(nid).sea_recruits);
    }
    std::printf("SEED %u: nations %d (majors %d) | ports %d, lake %d | no port %d, no sea port %d "
                "(majors %d) | capital sea port %d (majors %d)\n",
                seed, t.nations, t.majors, t.ports, t.lake_ports, t.no_port, t.no_sea_port,
                t.majors_no_sea_port, t.capital_sea_port, t.majors_capital_sea_port);
    std::fflush(stdout);
    all.nations += t.nations; all.majors += t.majors; all.ports += t.ports; all.lake_ports += t.lake_ports;
    all.no_port += t.no_port; all.no_sea_port += t.no_sea_port; all.majors_no_sea_port += t.majors_no_sea_port;
    all.capital_sea_port += t.capital_sea_port; all.majors_capital_sea_port += t.majors_capital_sea_port;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) seeds = parse_seed_list(argv[++i]);
        else { std::fprintf(stderr, "usage: port_mouth_census [--seeds a,b]\n"); return 2; }
    }
    if (seeds.empty())
        seeds = library_seeds("docs/generation/seed_library.json");
    tally all;
    for (const std::uint32_t s : seeds)
        run_seed(s, all);
    std::printf("POOLED: nations %d (majors %d) | ports %d, lake %d | no port %d, no sea port %d "
                "(majors %d) | capital sea port %d (majors %d)\n",
                all.nations, all.majors, all.ports, all.lake_ports, all.no_port, all.no_sea_port,
                all.majors_no_sea_port, all.capital_sea_port, all.majors_capital_sea_port);
    return 0;
}
