// garrison_border_probe — what a garrison's POST does to the nation AI's
// force reading (BL-1145 review, 2026-10-03).
//
// WHY THIS EXISTS. The nation scorer (nation_ai.cpp `build_force`) counts a
// unit as border force only through its own tile's hex neighbours: a garrison
// standing deep inside its province faces no one, so where a garrison is
// POSTED is read by the threat term, the threat budget lines and, through the
// budget, the settled world. Ben ruled (2026-10-03) that a border garrison
// stands on its province's tile nearest the neighbour it guards and the
// capital garrison on the capital province's anchor centre; this probe is the
// before/after reading for that move. It REPORTS, and gates nothing — C16 in
// centre_census is the rule's assertion.
//
// Per seed, on the shipped start (harness_params' build_app_start_world):
//   * garrisons      nation-owned units, and how many FACE a foreign nation
//                    (a hex neighbour of their tile is another nation's) —
//                    the units build_force puts in a border[N][home] cell;
//   * border cells   distinct (threatened, home) pairs carrying garrison
//                    force, and carrying any unit's force;
//   * the scorer     score_national_budgets over one full cadence (every
//                    nation scored once): sum of border_force, sum of threat,
//                    nations with threat > 0, the summed weights of the three
//                    threat lines (contracted_force, strategic_reserve,
//                    military_research), and an FNV-1a digest of every weight;
//   * D_settle       world_digest.hpp's recipe after the app's validation run.
//
// Run from the repo root: garrison_border_probe [--seeds a,b,c] [--no-settle]
// Seeds default to docs/generation/seed_library.json.

#include "harness_params.hpp"
#include "world_digest.hpp"
#include "scripting/lua_state.hpp"
#include "world/hex_neighbors.hpp"
#include "world/logistics.hpp"     // body_tile_grid
#include "world/nation_ai.hpp"
#include "world/nation_budget.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{

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
        if (!tok.empty())
            out.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
    return out;
}

entity_id nation_of(const world& w, entity_id tile)
{
    const auto it = w.tile_to_nation.find(tile);
    return it == w.tile_to_nation.end() ? null_entity : it->second;
}

/// The foreign nations a unit on @p tile touches, ascending — build_force's walk.
std::vector<entity_id> touched(world& w, entity_id tile)
{
    std::vector<entity_id> out;
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end()) return out;
    const auto bit = w.bodies.find(tit->second.body);
    if (bit == w.bodies.end()) return out;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    const std::vector<entity_id>& grid = body_tile_grid(w, tit->second.body);
    const entity_id home = nation_of(w, tile);
    for (int side = 0; side < 6; ++side)
    {
        const hex_neighbors::coord c = hex_neighbors::neighbour(tit->second.grid_x, tit->second.grid_y, side);
        if (c.gy < 0 || c.gy >= gh) continue;
        const int nx = ((c.gx % gw) + gw) % gw;
        const std::size_t idx = static_cast<std::size_t>(c.gy) * gw + nx;
        if (idx >= grid.size() || grid[idx] == null_entity) continue;
        const entity_id other = nation_of(w, grid[idx]);
        if (other == null_entity || other == home) continue;
        out.push_back(other);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

struct fnv
{
    uint64_t h = 1469598103934665603ull;
    void bytes(const void* p, std::size_t n)
    {
        const auto* b = static_cast<const unsigned char*>(p);
        for (std::size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 1099511628211ull; }
    }
};

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    bool settle = true;
    for (int a = 1; a < argc; ++a)
    {
        const std::string s = argv[a];
        if (s == "--seeds" && a + 1 < argc) seeds = parse_seed_list(argv[++a]);
        else if (s == "--no-settle")        settle = false;
        else { std::printf("usage: garrison_border_probe [--seeds a,b,c] [--no-settle]\n"); return 2; }
    }
    if (seeds.empty()) seeds = library_seeds("docs/generation/seed_library.json");
    if (seeds.empty()) { std::printf("FATAL  no seeds (run from the repo root)\n"); return 2; }

    std::printf("garrison_border_probe — the shipped start (build_app_start_world); a reading\n");
    std::printf("seed  garrisons  facing | cells_garrison  cells_all | border_force       threat  threatened"
                " | contracted  reserve  milres | weights_digest    | D_settle\n");

    lua_state lua;
    for (const uint32_t seed : seeds)
    {
        world_params params{};
        params.seed = seed;
        app_start_world out;
        build_app_start_world(lua, params, out);
        world& w = out.w; // non-const: body_tile_grid builds its raster lazily

        // --- garrisons and border cells --------------------------------------
        std::vector<entity_id> uids;
        for (const auto& kv : w.units) uids.push_back(kv.first);
        std::sort(uids.begin(), uids.end());
        int garrisons = 0, facing = 0;
        std::set<std::pair<entity_id, entity_id>> cells_g, cells_all;
        for (const entity_id uid : uids)
        {
            const unit_component& u = w.units.at(uid);
            if (u.count <= 0 || u.position == null_entity) continue;
            const bool is_garrison = w.nations.count(u.owner) != 0;
            const std::vector<entity_id> t = touched(w, u.position);
            const entity_id home = nation_of(w, u.position);
            if (is_garrison) { ++garrisons; if (!t.empty()) ++facing; }
            for (const entity_id thr : t)
            {
                cells_all.insert({ thr, home });
                if (is_garrison) cells_g.insert({ thr, home });
            }
        }

        // --- the scorer, one full cadence ------------------------------------
        const nation_ai_params p{};
        double border_force = 0.0, threat = 0.0, contracted = 0.0, reserve = 0.0, milres = 0.0;
        int threatened = 0;
        fnv dig;
        for (int tick = 0; tick < p.cadence_k; ++tick)
        {
            nation_scorer_report rep;
            const std::map<entity_id, nation_budget> b = score_national_budgets(w, p, tick, &rep);
            for (const nation_score_terms& t : rep.scored)
            {
                border_force += t.border_force;
                threat += t.threat;
                if (t.threat > 0.0f) ++threatened;
            }
            for (const auto& [nid, nb] : b) // std::map: ascending nation id
            {
                dig.bytes(&nid, sizeof nid);
                dig.bytes(nb.weights.data(), sizeof(float) * nb.weights.size());
                dig.bytes(&nb.reserve_fraction, sizeof nb.reserve_fraction);
                contracted += nb.weights[static_cast<std::size_t>(budget_priority::contracted_force)];
                reserve    += nb.weights[static_cast<std::size_t>(budget_priority::strategic_reserve)];
                milres     += nb.weights[static_cast<std::size_t>(budget_priority::military_research)];
            }
        }

        uint64_t d_settle = 0;
        if (settle)
        {
            run_app_validation_settle(out.w, out.reg);
            d_settle = world_state_digest(out.w);
        }

        std::printf("%4u  %9d  %6d | %14zu  %9zu | %12.1f  %11.4f  %10d | %10.4f  %7.4f  %6.4f | %016" PRIx64
                    "  | %016" PRIx64 "\n",
                    seed, garrisons, facing, cells_g.size(), cells_all.size(), border_force, threat,
                    threatened, contracted, reserve, milres, dig.h, d_settle);
        std::fflush(stdout);
    }
    return 0;
}
