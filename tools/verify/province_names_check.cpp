// ---------------------------------------------------------------------------
// province_names_check — every province has a name (BL-1708 province names)
// ---------------------------------------------------------------------------
// PROVINCES.md § A province's name (Ben, 2026-11-19). Builds the SHIPPED world
// (seed 0 unless --seed N; --data loads the shipped data layer first, as the app
// does) and checks what generation wrote into `world::province_name`:
//
//   P1  Every province of the partition has a name, non-empty and within
//       `k_province_name_max_bytes`; no name belongs to an id that is not one.
//   P2  A province holding a population centre carries the name of the lowest-id
//       centre in it, verbatim.
//   P3  Names are unique on their body.
//   P4  Determinism: the same seed builds the same names, entry for entry.
//   P5  The save carries them: today's (v98) stream round-trips every name.
//   P6  A v97 stream (no name section) loads with no stored names, and
//       `province_display_name` then reads each centre province's centre name.
//   P7  A hostile v98 stream is refused whole: a name for an id that is not a
//       province, and an over-long name.
//
// Reported beside the checks: provinces, how many took a centre's name, how many
// coined, and a sample of each. Exits non-zero on any FAIL.
//
//   bash tools/verify/build_lua_harness.sh province_names_check --run
//   build_gen/verify/province_names_check.exe [--seed N] [--data]

#include "harness_params.hpp"
#include "world/city_names.hpp"
#include "world/components.hpp"
#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/province.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"
#include "world/world_save.hpp"
#include "world/works_roster.hpp"
#include "scripting/lua_state.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool cond, const char* what)
{
    std::printf("  %s  %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) ++g_failures;
}

std::string to_bytes(const world& w)
{
    std::ostringstream out(std::ios::binary);
    write_world_snapshot(w, out);
    return out.str();
}

bool from_bytes(const std::string& b, world& w)
{
    std::istringstream in(b, std::ios::binary);
    return read_world_snapshot(w, in);
}

world build(uint32_t seed, bool data)
{
    lua_state        lua;
    world_gen_config cfg{};
    works_registry   works;
    if (data)
    {
        lua.load("scripts/world_gen.lua");
        cfg.load_from_lua(lua);
        lua.load("scripts/works.lua");
        works.load_from_lua(lua);
    }
    world_params p = arc_params(world_arc::shipped);
    p.seed         = seed;
    generation_report     rep{};
    era_minus_one_fixture fx{};
    return data ? make_hard_coded_world(p, &rep, cfg, nullptr, &works, &fx)
                : make_hard_coded_world(p, &rep, {}, nullptr, nullptr, &fx);
}

} // namespace

int main(int argc, char** argv)
{
    uint32_t seed = 0;
    bool     data = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
            seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        else if (std::strcmp(argv[i], "--data") == 0)
            data = true;
    }
    std::printf("province_names_check: seed %u%s\n", seed, data ? " (shipped data)" : "");
    const world w = build(seed, data);

    // The lowest centre per province.
    std::map<std::uint32_t, entity_id> centre_of;
    for (const auto& [cid, ctile] : w.population_centre_tile)
    {
        const std::uint32_t pid = w.provinces.province_of(ctile);
        if (pid == 0) continue;
        auto [it, fresh] = centre_of.emplace(pid, cid);
        if (!fresh && cid < it->second) it->second = cid;
    }

    int named = 0, unnamed = 0, too_long = 0, from_centre = 0, centre_mismatch = 0, coined = 0;
    std::map<entity_id, std::set<std::string>> per_body;
    int dupes = 0;
    std::string sample_centre, sample_coined;
    std::set<std::uint32_t> ids;
    for (const province& pr : w.provinces.provinces)
    {
        ids.insert(pr.id);
        const auto it = w.province_name.find(pr.id);
        if (it == w.province_name.end() || it->second.empty())
        {
            ++unnamed;
            continue;
        }
        ++named;
        if (it->second.size() > k_province_name_max_bytes) ++too_long;
        if (!per_body[pr.body].insert(it->second).second) ++dupes;
        if (const auto c = centre_of.find(pr.id); c != centre_of.end())
        {
            ++from_centre;
            const auto nm = w.population_centre_name.find(c->second);
            if (nm == w.population_centre_name.end() || nm->second != it->second) ++centre_mismatch;
            if (sample_centre.empty()) sample_centre = it->second;
        }
        else
        {
            ++coined;
            if (sample_coined.empty()) sample_coined = it->second;
        }
    }
    int stray = 0;
    for (const auto& [pid, nm] : w.province_name)
        if (ids.count(pid) == 0) ++stray;

    std::printf("  provinces %zu: %d named (%d from a centre, %d coined), %d unnamed\n",
                w.provinces.provinces.size(), named, from_centre, coined, unnamed);
    std::printf("  sample: centre province \"%s\", coined province \"%s\"\n",
                sample_centre.c_str(), sample_coined.c_str());

    check(!w.provinces.provinces.empty() && unnamed == 0 && too_long == 0 && stray == 0,
          "P1 every province is named, within the byte cap, and no name names a non-province");
    check(from_centre > 0 && centre_mismatch == 0,
          "P2 a centre's province carries its lowest centre's name, verbatim");
    check(dupes == 0, "P3 names are unique on their body");

    {
        const world again = build(seed, data);
        bool same = again.province_name.size() == w.province_name.size();
        for (const auto& [pid, nm] : w.province_name)
        {
            const auto it = again.province_name.find(pid);
            same = same && it != again.province_name.end() && it->second == nm;
        }
        check(same, "P4 the same seed builds the same names");
    }

    const std::string b = to_bytes(w);
    {
        world r;
        const bool ok = from_bytes(b, r);
        bool same = ok && r.province_name.size() == w.province_name.size();
        if (same)
            for (const auto& [pid, nm] : w.province_name)
                same = same && r.province_name.count(pid) == 1 && r.province_name.at(pid) == nm;
        check(same && to_bytes(r) == b, "P5 today's stream round-trips every name, byte-identical");
    }

    {
        std::ostringstream o96(std::ios::binary);
        write_world_snapshot_pre_province_names(w, o96);
        world r;
        const bool ok = from_bytes(o96.str(), r);
        bool reads = ok && r.province_name.empty()
                     && o96.str().size() + world_save_province_names_section_bytes(w) == b.size();
        int checked = 0;
        if (reads)
            for (const auto& [pid, cid] : centre_of)
            {
                reads = reads && province_display_name(r, pid) == w.population_centre_name.at(cid);
                ++checked;
            }
        check(reads && checked > 0,
              "P6 a v97 stream loads with no stored names, and each centre province reads its centre's");
    }

    {
        // Hostile streams: rewrite the section's first record. Layout at the tail:
        // u32 count, then (u32 id, u32 len, bytes) per name, ascending id.
        const std::size_t at = b.size() - world_save_province_names_section_bytes(w);
        std::string bad_id = b;
        const uint32_t not_a_province = 0xFFFFFFF0u;
        std::memcpy(&bad_id[at + 4], &not_a_province, sizeof not_a_province);
        world r1;
        const bool refused_id = !from_bytes(bad_id, r1) && r1.tiles.empty();

        std::string bad_len = b;
        const uint32_t long_len = k_province_name_max_bytes + 1;
        std::memcpy(&bad_len[at + 8], &long_len, sizeof long_len);
        world r2;
        const bool refused_len = !from_bytes(bad_len, r2) && r2.tiles.empty();
        check(refused_id && refused_len,
              "P7 a name for a non-province, and an over-long name, are each refused whole");
    }

    std::printf(g_failures == 0 ? "\nALL PASS\n" : "\n%d FAILURE(S)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
