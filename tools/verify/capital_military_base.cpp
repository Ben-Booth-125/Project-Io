// capital_military_base — BL-1439 (capital military base; Ben, 2026-10-27:
// "make sure that the capital is generated with a military base";
// MILITARY.md § Nation garrisons).
//
// Per seed, on the shipped start (harness_params' build_app_start_world, the
// finish_campaign_world mirror, before the settle):
//   C1 every nation with ground owns exactly ONE completed, not-decommissioned
//      military_base in its capital province, on its own ground, on a tile
//      `placement_rules::can_place` accepts, and no other military base;
//   C2 the base is on the road network (PRODUCTION.md § A building needs a road);
//   C3 the capital garrison (the nation's unit in its capital province that
//      was not raised at a base) records that base as its muster_base, and
//      every border garrison still records none;
//   C4 the nation's base list and the capital-province fallback are reported:
//      a nation whose capital province had no legal tile is NAMED, not excused.
// Once, on the first seed:
//   C5 a picked seat (corp_verb::take_seat) keeps its capital base and its
//      capital garrison, and loses only the force RAISED at the base; the seat it
//      left is armed at its own capital base (one base, not two).
//
// Run from the repo root: capital_military_base [--seeds a,b,c]
// Seeds default to docs/generation/seed_library.json. Needs a live Lua state
// (build_lua_harness).

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/corp_command.hpp"
#include "world/corporation_generation.hpp" // unit_is_raised
#include "world/placement_rules.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

int g_failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++g_failures;
}

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

std::vector<uint32_t> parse_seed_list(const std::string& s)
{
    std::vector<uint32_t> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ','))
        if (!tok.empty()) out.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
    return out;
}

uint32_t capital_province(const world& w, entity_id nid)
{
    const nation_component& nc = w.nations.at(nid);
    return nc.capital_tile != null_entity ? w.provinces.province_of(nc.capital_tile) : 0u;
}

/// The nation's military bases, ascending id.
std::vector<entity_id> bases_of(const world& w, entity_id nid)
{
    std::vector<entity_id> out;
    for (const entity_id bid : w.nations.at(nid).state_assets)
        if (const auto b = w.buildings.find(bid);
            b != w.buildings.end() && b->second.type == building_type::military_base)
            out.push_back(bid);
    std::sort(out.begin(), out.end());
    return out;
}

/// The capital garrison: the lowest-id unit of @p nid in its capital province
/// that was not raised at a base.
entity_id capital_garrison(const world& w, entity_id nid)
{
    const uint32_t cp = capital_province(w, nid);
    entity_id best = null_entity;
    if (cp == 0) return best;
    for (const auto& [uid, u] : w.units)
        if (u.owner == nid && !unit_is_raised(uid, u) && w.provinces.province_of(u.position) == cp
            && (best == null_entity || uid < best))
            best = uid;
    return best;
}

int raised_units(const world& w, entity_id nid)
{
    int n = 0;
    for (const auto& [uid, u] : w.units)
        if (u.owner == nid && unit_is_raised(uid, u)) ++n;
    return n;
}

struct seed_row
{
    int nations = 0, ok = 0, outside = 0, none = 0, extra = 0, unroaded = 0,
        garrison_ok = 0, garrisonless = 0, border_bad = 0, none_justified = 0;
};

seed_row check_world(const world& w, uint32_t seed)
{
    seed_row r;
    std::vector<entity_id> ids;
    for (const auto& kv : w.nations) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    for (const entity_id nid : ids)
    {
        const nation_component& nc = w.nations.at(nid);
        if (nc.tiles.empty()) continue;
        ++r.nations;
        const uint32_t cp = capital_province(w, nid);
        const std::vector<entity_id> bases = bases_of(w, nid);
        int in_capital = 0;
        entity_id cap_base = null_entity;
        bool legal = true, roaded = true;
        for (const entity_id bid : bases)
        {
            const building_component& b = w.buildings.at(bid);
            const auto own = w.tile_to_nation.find(b.tile);
            const bool own_ground = own != w.tile_to_nation.end() && own->second == nid;
            if (b.ticks_remaining <= 0 && !b.decommissioned && own_ground && cp != 0
                && w.provinces.province_of(b.tile) == cp)
            {
                ++in_capital;
                if (cap_base == null_entity) cap_base = bid;
                const tile_component& tc = w.tiles.at(b.tile);
                bool any = false;
                const resource_type tgt = placement_rules::richest_extractable(tc, any);
                legal = static_cast<bool>(placement_rules::can_place(tc, building_type::military_base, tgt));
                roaded = tile_has_road(tc);
            }
        }
        if (bases.size() > 1) ++r.extra;
        if (in_capital == 1 && bases.size() == 1 && legal)
            ++r.ok;
        else if (bases.empty())
        {
            ++r.none;
            // Why: the nation's tiles, the legal ones (can_place), and how many
            // of those still have room under the ordinary rules.
            int legal_tiles = 0, legal_free = 0, in_cap = 0;
            for (const entity_id tid : nc.tiles)
            {
                if (w.provinces.province_of(tid) == cp) ++in_cap;
                const tile_component& tc = w.tiles.at(tid);
                bool any = false;
                const resource_type tgt = placement_rules::richest_extractable(tc, any);
                if (!placement_rules::can_place(tc, building_type::military_base, tgt)) continue;
                ++legal_tiles;
                bool occupied = false;
                for (const auto& [bid, b] : w.buildings)
                    if (b.tile == tid)
                    {
                        occupied = true;
                        std::printf("    tile %u holds building %u type %d, ticks_remaining %d "
                                    "(non-extraction cap %d)\n", static_cast<unsigned>(tid),
                                    static_cast<unsigned>(bid), static_cast<int>(b.type),
                                    b.ticks_remaining,
                                    placement_rules::non_extraction_stack_cap(tc.substrate, tc.cover));
                    }
                if (!occupied) ++legal_free;
                else if (placement_rules::non_extraction_buildings_on_tile(w, tid)
                             < placement_rules::non_extraction_stack_cap(tc.substrate, tc.cover)
                         && !placement_rules::tile_has_site_under_construction(w, tid))
                    ++legal_free; // the ordinary stacking rule still has room
            }
            // The doc's rule: a nation with NO legal tile at all opens with no base.
            // Justified only when the harness finds none independently.
            if (legal_free == 0) ++r.none_justified;
            std::printf("  seed %u nation %u \"%s\": NO military base (%zu tile(s), %d in the "
                        "capital province, %d legal, %d legal with room)\n", seed,
                        static_cast<unsigned>(nid), nc.name.c_str(), nc.tiles.size(), in_cap,
                        legal_tiles, legal_free);
        }
        else if (in_capital == 0)
        {
            ++r.outside;
            std::printf("  seed %u nation %u \"%s\": base outside the capital province (%zu base(s))\n",
                        seed, static_cast<unsigned>(nid), nc.name.c_str(), bases.size());
        }
        else
            std::printf("  seed %u nation %u \"%s\": %d capital base(s), %zu base(s) in all, legal %d\n",
                        seed, static_cast<unsigned>(nid), nc.name.c_str(), in_capital, bases.size(),
                        legal ? 1 : 0);
        if (cap_base != null_entity && !roaded)
        {
            ++r.unroaded;
            std::printf("  seed %u nation %u: capital base %u stands on a roadless tile\n", seed,
                        static_cast<unsigned>(nid), static_cast<unsigned>(cap_base));
        }
        // C3: the capital garrison names the base; border garrisons name none.
        const entity_id g = capital_garrison(w, nid);
        if (g == null_entity)
            ++r.garrisonless;
        else if (cap_base != null_entity && w.units.at(g).muster_base == cap_base)
            ++r.garrison_ok;
        else
            std::printf("  seed %u nation %u: capital garrison %u records muster_base %u (base %u)\n",
                        seed, static_cast<unsigned>(nid), static_cast<unsigned>(g),
                        static_cast<unsigned>(w.units.at(g).muster_base),
                        static_cast<unsigned>(cap_base));
        for (const auto& [uid, u] : w.units)
            if (u.owner == nid && uid != g && !unit_is_raised(uid, u) && u.muster_base != null_entity)
                ++r.border_bad;
    }
    return r;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    for (int a = 1; a < argc; ++a)
    {
        const std::string s = argv[a];
        if (s == "--seeds" && a + 1 < argc) seeds = parse_seed_list(argv[++a]);
        else { std::printf("usage: capital_military_base [--seeds a,b,c]\n"); return 2; }
    }
    if (seeds.empty()) seeds = library_seeds("docs/generation/seed_library.json");
    if (seeds.empty()) { std::printf("FATAL  no seeds (run from the repo root)\n"); return 2; }

    std::printf("capital_military_base — BL-1439, the shipped start (build_app_start_world)\n");
    lua_state lua;
    seed_row total;
    bool first = true;
    for (const uint32_t seed : seeds)
    {
        world_params params{};
        params.seed = seed;
        app_start_world out;
        build_app_start_world(lua, params, out);
        world& w = out.w;
        const seed_row r = check_world(w, seed);
        std::printf("[capital_military_base] seed %u: %d nations, %d one-capital-base, %d outside, "
                    "%d none, %d with >1 base, %d unroaded; garrisons %d recorded, %d nations "
                    "without one, %d border garrison(s) with a base\n",
                    seed, r.nations, r.ok, r.outside, r.none, r.extra, r.unroaded, r.garrison_ok,
                    r.garrisonless, r.border_bad);
        std::fflush(stdout);
        total.nations += r.nations; total.ok += r.ok; total.outside += r.outside;
        total.none += r.none; total.extra += r.extra; total.unroaded += r.unroaded;
        total.garrison_ok += r.garrison_ok; total.garrisonless += r.garrisonless;
        total.border_bad += r.border_bad;
        total.none_justified += r.none_justified;

        if (first)
        {
            first = false;
            // C5: a picked seat keeps its capital base and garrison.
            entity_id pick = null_entity;
            std::vector<entity_id> ids;
            for (const auto& kv : w.nations) ids.push_back(kv.first);
            std::sort(ids.begin(), ids.end());
            for (const entity_id nid : ids)
                if (nid != w.player_entity && w.nations.at(nid).is_major && raised_units(w, nid) > 0)
                {
                    pick = nid;
                    break;
                }
            const entity_id old_seat = w.player_entity;
            const std::vector<entity_id> pick_bases = pick != null_entity ? bases_of(w, pick)
                                                                           : std::vector<entity_id>{};
            const entity_id pick_garrison = pick != null_entity ? capital_garrison(w, pick) : null_entity;
            corp_command cmd;
            cmd.corp = pick;
            cmd.verb = corp_verb::take_seat;
            const bool applied = pick != null_entity
                && apply_corp_command(w, out.reg, cmd) == corp_command_result::applied;
            const bool kept = applied && bases_of(w, pick) == pick_bases && pick_bases.size() == 1
                && w.units.count(pick_garrison) != 0
                && w.units.at(pick_garrison).muster_base == pick_bases.front();
            const bool unarmed = applied && raised_units(w, pick) == 0;
            const bool old_armed = old_seat != null_entity && w.nations.count(old_seat) != 0
                && bases_of(w, old_seat).size() == 1 && raised_units(w, old_seat) == 1;
            std::printf("  C5 seed %u: picked %u over seat %u -> applied %d, bases %zu, raised %d; "
                        "the seat left holds %zu base(s), %d raised\n",
                        seed, static_cast<unsigned>(pick), static_cast<unsigned>(old_seat),
                        applied ? 1 : 0, pick != null_entity ? bases_of(w, pick).size() : 0u,
                        pick != null_entity ? raised_units(w, pick) : -1,
                        old_seat != null_entity && w.nations.count(old_seat) ? bases_of(w, old_seat).size() : 0u,
                        old_seat != null_entity ? raised_units(w, old_seat) : -1);
            check(kept, "C5 a picked seat keeps its one capital base and its capital garrison's muster_base");
            check(unarmed, "C5 a picked seat loses the force raised at the base");
            check(old_armed, "C5 the seat it left is armed at its one capital base");
        }
    }
    std::printf("\n[capital_military_base] TOTAL %zu seeds: %d nations, %d with one capital base, "
                "%d outside the capital province, %d with none (%d with no legal tile at all), "
                "%d with >1 base, %d unroaded; "
                "%d capital garrisons recorded, %d nations without a capital garrison, %d border "
                "garrison(s) with a base\n",
                seeds.size(), total.nations, total.ok, total.outside, total.none,
                total.none_justified, total.extra,
                total.unroaded, total.garrison_ok, total.garrisonless, total.border_bad);
    check(total.nations > 0 && total.ok + total.none_justified == total.nations
              && total.none == total.none_justified,
          "C1 every nation owns exactly one completed military base, in its capital province, on its own "
          "ground, on a legal tile - or has no legal tile at all (named above)");
    check(total.unroaded == 0, "C2 every capital base stands on the road network");
    check(total.garrison_ok + total.garrisonless + total.none_justified == total.nations
              && total.garrison_ok > 0,
          "C3 every capital garrison of a nation with a base records it as muster_base");
    check(total.border_bad == 0, "C3 no border garrison records a muster base");
    std::printf("\n%s (%d failures)\n", g_failures == 0 ? "ALL PASS" : "FAILURES", g_failures);
    return g_failures == 0 ? 0 : 1;
}
