// ---------------------------------------------------------------------------
// opening_parity — BL-1475 (opening parity census), a pure reader
// ---------------------------------------------------------------------------
// The measurement for the major opening kit (CORPORATION_GENERATION.md § A
// major's opening kit, Ben 2026-11-02): how equal are the majors' openings, and
// do they stay workable over several years of play? Built as nation_sizes
// builds it (make_hard_coded_world with the wizard's nation targets, then
// finish_campaign_world: the search, the fold, the opening reserve and cut, and
// the 12-tick settle), then stepped N play quarters with NOBODY seated
// (spectating), so every major is run by its own scorer on equal terms. The
// seat is deliberately not taken: a seat stops its nation's scorer, which would
// read as that major falling behind.
//
// PER SEED AND TARGET PAIR it prints, for every MAJOR:
//   * at the HANDOFF: tiles, treasury, completed buildings by category
//     (extraction / processing / logistics / civic / military; under
//     construction apart), units and men, the land recruit pool;
//   * over the play quarters: the filed quarterly return's net each quarter
//     (mean, min, quarters in the red), the treasury at the end, buildings and
//     men at the end;
//   * SPREAD across the majors: max/min of handoff treasury, buildings, men and
//     mean net (a ratio of 1.00 is perfect parity);
//   * SPACING: how many major pairs share a border (two tiles on one body within
//     one step of each other on the grid, 8-neighbourhood, no wrap — a
//     conservative stand-in for hex adjacency that may count a corner touch);
//   * the minors in one line: count, median treasury, median buildings, median men.
// Machine-readable lines start "ROW" (one per major) and "SEED" (one per world),
// so a par_seeds sweep can be pooled from its shards.
//
// Usage: build_gen/verify/opening_parity.exe [--seeds a,b] [--pairs 20:4,30:4] [--quarters N]
//                                            [--kit <name>|none]
// --kit (BL-1473) overrides world_gen.opening_kit.active before generation: a named
// configuration, or `none` for the walk's old opening. Each major line then carries
// the kit report's totals (placed / substituted / unbuilt), as do its ROW fields.
// Build: bash tools/verify/build_lua_harness.sh opening_parity
// ---------------------------------------------------------------------------

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/finish_campaign_world.hpp"
#include "world/hard_coded_world.hpp"
#include "world/orbital_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/survey_system.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"
#include "world/works_roster.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
std::vector<int> parse_ints(const char* s, char sep)
{
    std::vector<int> out;
    std::string cur;
    for (const char* p = s;; ++p)
    {
        if (*p == sep || *p == '\0')
        {
            if (!cur.empty()) out.push_back(std::atoi(cur.c_str()));
            cur.clear();
            if (*p == '\0') break;
        }
        else cur.push_back(*p);
    }
    return out;
}

enum category { k_extraction, k_processing, k_logistics, k_civic, k_military, k_other, k_category_count };
const char* const k_category_names[k_category_count] = { "ext", "proc", "logi", "civic", "mil", "other" };

category category_of(building_type t)
{
    switch (t)
    {
    case building_type::extraction_site:       return k_extraction;
    case building_type::processing_facility:   return k_processing;
    case building_type::port:
    case building_type::launchpad:
    case building_type::inland_logistics_hub:
    case building_type::railway_station:       return k_logistics;
    case building_type::research_institute:
    case building_type::schooling:
    case building_type::university:
    case building_type::planetary_marketplace: return k_civic;
    case building_type::military_base:
    case building_type::airbase:
    case building_type::bunker:                return k_military;
    default:                                   return k_other;
    }
}

struct snapshot
{
    int    tiles = 0;
    double treasury = 0.0;
    int    built[k_category_count] = {};
    int    built_all = 0, constructing = 0;
    int    idled = 0; ///< BL-1473 census: decommissioned (idled) state buildings, counted apart
    int    units = 0;
    long   men = 0;
    int    land_pool = 0;
};

snapshot read_nation(const world& w, entity_id nid, const nation_component& n,
                     const std::map<entity_id, std::pair<int, long>>& force)
{
    snapshot s;
    s.tiles = static_cast<int>(n.tiles.size());
    s.treasury = n.treasury;
    s.land_pool = n.land_recruits;
    for (const entity_id a : n.state_assets)
    {
        const auto it = w.buildings.find(a);
        if (it == w.buildings.end()) continue;
        if (it->second.decommissioned) { ++s.idled; continue; }
        if (it->second.ticks_remaining > 0) { ++s.constructing; continue; }
        ++s.built[category_of(it->second.type)];
        ++s.built_all;
    }
    if (const auto f = force.find(nid); f != force.end()) { s.units = f->second.first; s.men = f->second.second; }
    return s;
}

std::map<entity_id, std::pair<int, long>> force_by_owner(const world& w)
{
    std::map<entity_id, std::pair<int, long>> out;
    for (const auto& [uid, u] : w.units)
    {
        auto& f = out[u.owner];
        ++f.first;
        f.second += u.count;
    }
    return out;
}

/// Major pairs sharing a border: any two tiles on one body, owned by two
/// different majors, within one grid step (8-neighbourhood, no wrap).
int touching_major_pairs(const world& w, const std::set<entity_id>& majors)
{
    std::map<std::tuple<entity_id, int, int>, entity_id> at; // (body, x, y) -> major
    for (const auto& [tid, nid] : w.tile_to_nation)
    {
        if (!majors.count(nid)) continue;
        const auto t = w.tiles.find(tid);
        if (t == w.tiles.end()) continue;
        at[{ t->second.body, t->second.grid_x, t->second.grid_y }] = nid;
    }
    std::set<std::pair<entity_id, entity_id>> pairs;
    for (const auto& [key, nid] : at)
    {
        const auto& [body, x, y] = key;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (!dx && !dy) continue;
                const auto o = at.find({ body, x + dx, y + dy });
                if (o == at.end() || o->second == nid) continue;
                pairs.insert({ std::min(nid, o->second), std::max(nid, o->second) });
            }
    }
    return static_cast<int>(pairs.size());
}

template <class T> T median_of(std::vector<T> v)
{
    if (v.empty()) return T{};
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

double ratio(double hi, double lo) { return lo > 0.0 ? hi / lo : (hi > 0.0 ? 999.0 : 1.0); }
} // namespace

int main(int argc, char** argv)
{
    std::vector<int> seeds{ 0 };
    std::vector<std::pair<int, int>> pairs{ { 20, 4 } };
    int quarters = 20;
    const char* kit_override = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) seeds = parse_ints(argv[++i], ',');
        else if (!std::strcmp(argv[i], "--quarters") && i + 1 < argc) quarters = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--kit") && i + 1 < argc) kit_override = argv[++i];
        else if (!std::strcmp(argv[i], "--pairs") && i + 1 < argc)
        {
            pairs.clear();
            const std::string all = argv[++i];
            std::size_t at = 0;
            while (at < all.size())
            {
                const std::size_t comma = all.find(',', at);
                const std::string one = all.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
                const std::size_t colon = one.find(':');
                if (colon != std::string::npos)
                    pairs.emplace_back(std::atoi(one.substr(0, colon).c_str()), std::atoi(one.substr(colon + 1).c_str()));
                if (comma == std::string::npos) break;
                at = comma + 1;
            }
        }
        else
        {
            std::fprintf(stderr, "usage: opening_parity [--seeds a,b] [--pairs 20:4,30:4] [--quarters N] [--kit <name>|none]\n");
            return 2;
        }
    }

    lua_state        lua;
    world_gen_config cfg{};
    works_registry   works;
    recipe_registry  reg;
    lua.load("scripts/world_gen.lua");
    cfg.load_from_lua(lua);
    if (kit_override != nullptr)
    {
        cfg.opening_kit.active = kit_override;
        if (std::strcmp(kit_override, "none") != 0 && cfg.opening_kit.active_config() == nullptr)
        {
            std::fprintf(stderr, "opening_parity: no opening kit named '%s' in scripts/world_gen.lua\n",
                         kit_override);
            return 2;
        }
    }
    lua.load("scripts/works.lua");
    works.load_from_lua(lua);
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    reg.load_from_lua(lua);

    std::printf("opening_parity - BL-1475: majors at the handoff and over %d play quarters (spectating); kit %s\n",
                quarters, cfg.opening_kit.active_config() ? cfg.opening_kit.active.c_str() : "none");
    for (const int seed : seeds)
        for (const auto& [tt, mt] : pairs)
        {
            world_params p = arc_params(world_arc::shipped);
            p.seed                = static_cast<uint32_t>(seed);
            p.nation_total_target = tt;
            p.major_target        = mt;
            generation_report rep{};
            world w = make_hard_coded_world(p, &rep, cfg, nullptr, &works);
            recipe_registry seed_reg = reg;
            const finish_campaign_result fin = finish_campaign_world(w, rep, seed_reg, p, cfg, nullptr);

            entity_id home = null_entity;
            for (const generation_report::body_entry& b : rep.bodies)
                if (b.is_homeworld) { home = b.id; break; }

            std::vector<entity_id> majors, minors;
            for (const auto& [nid, n] : w.nations)
            {
                if (n.tiles.empty()) continue;
                const auto t = w.tiles.find(n.tiles.front());
                if (t == w.tiles.end() || t->second.body != home) continue;
                (n.is_major ? majors : minors).push_back(nid);
            }
            std::sort(majors.begin(), majors.end());
            const std::set<entity_id> major_set(majors.begin(), majors.end());

            auto force = force_by_owner(w);
            std::map<entity_id, snapshot> at_handoff;
            for (const entity_id nid : majors) at_handoff[nid] = read_nation(w, nid, w.nations.at(nid), force);
            // BL-1473: the major's Logistic Point consumers without cover at the handoff.
            std::map<entity_id, int> uncovered;
            for (const entity_id nid : majors) uncovered[nid] = count_uncovered_buildings(w, nid);
            std::vector<double> minor_treasury;
            std::vector<int>    minor_built;
            std::vector<long>   minor_men;
            for (const entity_id nid : minors)
            {
                const snapshot s = read_nation(w, nid, w.nations.at(nid), force);
                minor_treasury.push_back(s.treasury);
                minor_built.push_back(s.built_all);
                minor_men.push_back(s.men);
            }
            const int touching = touching_major_pairs(w, major_set);

            // Play: N quarters, nobody seated (every nation's scorer acts).
            constexpr int k_econ_tick_days = 90;
            std::map<entity_id, std::vector<double>> nets;
            for (int k = 1; k <= quarters; ++k)
            {
                const int day = k * k_econ_tick_days;
                advance_orbits(w, static_cast<double>(k_econ_tick_days));
                advance_surveys(w, k_econ_tick_days);
                w.current_day_tick = day;
                (void)run_settle_tick(w, seed_reg, k_campaign_settle_ticks + (k - 1), day, /*spectating=*/true);
                for (const entity_id nid : majors)
                {
                    const auto it = w.nations.find(nid);
                    if (it == w.nations.end() || it->second.returns.empty()) continue;
                    nets[nid].push_back(it->second.returns.back().net);
                }
            }
            force = force_by_owner(w);

            std::printf("seed %d %d:%d  nations %zu (majors %zu, minors %zu)  majors touching %d pair(s)\n",
                        seed, tt, mt, majors.size() + minors.size(), majors.size(), minors.size(), touching);
            double t_hi = 0, t_lo = 1e30, n_hi = -1e30, n_lo = 1e30;
            int b_hi = 0, b_lo = 1 << 30;
            long long m_hi = 0, m_lo = 1LL << 40;
            for (const entity_id nid : majors)
            {
                const snapshot& h = at_handoff[nid];
                const auto e_it = w.nations.find(nid);
                const snapshot e = e_it == w.nations.end() ? snapshot{} : read_nation(w, nid, e_it->second, force);
                const std::vector<double>& q = nets[nid];
                double sum = 0.0, mn = q.empty() ? 0.0 : q.front();
                int red = 0;
                for (const double v : q) { sum += v; mn = std::min(mn, v); if (v < 0.0) ++red; }
                const double mean = q.empty() ? 0.0 : sum / static_cast<double>(q.size());
                std::printf("  major %llu  tiles %5d  treasury %9.0f  built %3d [", static_cast<unsigned long long>(nid),
                            h.tiles, h.treasury, h.built_all);
                for (int c = 0; c < k_category_count; ++c) std::printf("%s%s %d", c ? " " : "", k_category_names[c], h.built[c]);
                const opening_kit_major* km = fin.kit.of(nid);
                const int kp = km ? km->placed : 0, ks = km ? km->substituted : 0, ku = km ? km->unbuilt : 0;
                const int kpb = km ? km->proc_built : 0, unc = uncovered[nid];
                std::printf("] constr %d  units %d men %ld pool %d | kit %d/%d/%d proc %d uncovered %d idled %d | play:"
                            " net mean %8.1f min %8.1f red %2d/%zu  end treasury %9.0f built %3d men %ld\n",
                            h.constructing, h.units, h.men, h.land_pool, kp, ks, ku, kpb, unc, h.idled, mean, mn, red, q.size(),
                            e.treasury, e.built_all, e.men);
                std::printf("ROW seed=%d pair=%d:%d nation=%llu tiles=%d treasury=%.1f built=%d ext=%d proc=%d logi=%d"
                            " civic=%d mil=%d other=%d constr=%d units=%d men=%ld pool=%d net_mean=%.2f net_min=%.2f"
                            " red=%d quarters=%zu end_treasury=%.1f end_built=%d end_men=%ld"
                            " kit_placed=%d kit_sub=%d kit_unbuilt=%d proc_built=%d uncovered=%d idled=%d\n",
                            seed, tt, mt, static_cast<unsigned long long>(nid), h.tiles, h.treasury, h.built_all,
                            h.built[k_extraction], h.built[k_processing], h.built[k_logistics], h.built[k_civic],
                            h.built[k_military], h.built[k_other], h.constructing, h.units, h.men, h.land_pool,
                            mean, mn, red, q.size(), e.treasury, e.built_all, e.men, kp, ks, ku, kpb, unc, h.idled);
                t_hi = std::max(t_hi, h.treasury); t_lo = std::min(t_lo, h.treasury);
                b_hi = std::max(b_hi, h.built_all); b_lo = std::min(b_lo, h.built_all);
                m_hi = std::max(m_hi, static_cast<long long>(h.men)); m_lo = std::min(m_lo, static_cast<long long>(h.men));
                n_hi = std::max(n_hi, mean);        n_lo = std::min(n_lo, mean);
            }
            if (!majors.empty())
                std::printf("  spread (max/min): treasury %.2f  buildings %.2f  men %.2f  | net mean range %.1f .. %.1f\n",
                            ratio(t_hi, t_lo), ratio(b_hi, b_lo), ratio(static_cast<double>(m_hi), static_cast<double>(m_lo)),
                            n_lo, n_hi);
            std::printf("  minors: %zu  median treasury %.0f  median built %d  median men %ld\n", minors.size(),
                        median_of(minor_treasury), median_of(minor_built), median_of(minor_men));
            std::printf("SEED seed=%d pair=%d:%d majors=%zu minors=%zu touching=%d treasury_ratio=%.3f built_ratio=%.3f"
                        " men_ratio=%.3f net_lo=%.2f net_hi=%.2f minor_treasury_med=%.1f minor_built_med=%d"
                        " minor_men_med=%ld\n",
                        seed, tt, mt, majors.size(), minors.size(), touching, ratio(t_hi, t_lo), ratio(b_hi, b_lo),
                        ratio(static_cast<double>(m_hi), static_cast<double>(m_lo)), n_lo, n_hi,
                        median_of(minor_treasury), median_of(minor_built), median_of(minor_men));
            std::fflush(stdout);
        }
    return 0;
}
