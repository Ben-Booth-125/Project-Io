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
#include "world/building_profit.hpp"
#include "world/economy_system.hpp"
#include "world/finish_campaign_world.hpp"
#include "world/hard_coded_world.hpp"
#include "world/logistic_points.hpp"
#include "world/orbital_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/survey_system.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"
#include "world/works_roster.hpp"

#include <algorithm>
#include <array>
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

constexpr int k_econ_tick_days_diag = 90; ///< --diagnose-idle: the first play quarter's day

double ratio(double hi, double lo) { return lo > 0.0 ? hi / lo : (hi > 0.0 ? 999.0 : 1.0); }
} // namespace

int main(int argc, char** argv)
{
    std::vector<int> seeds{ 0 };
    std::vector<std::pair<int, int>> pairs{ { 20, 4 } };
    int quarters = 20;
    const char* kit_override = nullptr;
    bool diagnose_idle = false;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) seeds = parse_ints(argv[++i], ',');
        else if (!std::strcmp(argv[i], "--quarters") && i + 1 < argc) quarters = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--kit") && i + 1 < argc) kit_override = argv[++i];
        else if (!std::strcmp(argv[i], "--diagnose-idle")) diagnose_idle = true;
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
            // BL-1473 R4: which idled buildings at the handoff are kit slots, by kit category,
            // and the rest (logistics nodes, military, anything the kit did not lay).
            auto kit_idled = [&](entity_id nid, int out[4]) {
                out[0] = out[1] = out[2] = out[3] = 0;
                const opening_kit_major* km = fin.kit.of(nid);
                std::set<entity_id> kit_ids;
                if (km)
                    for (const opening_kit_slot_record& r : km->slots)
                    {
                        if (r.building == null_entity) continue;
                        kit_ids.insert(r.building);
                        const auto b = w.buildings.find(r.building);
                        if (b != w.buildings.end() && b->second.decommissioned)
                            ++out[static_cast<int>(r.category)];
                    }
                for (const entity_id a : w.nations.at(nid).state_assets)
                {
                    const auto b = w.buildings.find(a);
                    if (b != w.buildings.end() && b->second.decommissioned && !kit_ids.count(a)) ++out[3];
                }
            };
            std::map<entity_id, std::array<int, 4>> idled_at_handoff;
            for (const entity_id nid : majors)
            {
                kit_idled(nid, idled_at_handoff[nid].data());
                if (const opening_kit_major* km = fin.kit.of(nid))
                    for (const opening_kit_slot_record& r : km->slots)
                    {
                        const auto b = w.buildings.find(r.building);
                        if (r.building == null_entity || b == w.buildings.end() || !b->second.decommissioned) continue;
                        static const char* k_cat[] = {"extraction", "processing", "civic"};
                        std::printf("IDLEKIT seed=%d pair=%d:%d nation=%llu cat=%s slot=%s as=%s\n", seed, tt, mt,
                                    static_cast<unsigned long long>(nid), k_cat[static_cast<int>(r.category)],
                                    r.slot.c_str(), r.placed_as.empty() ? r.slot.c_str() : r.placed_as.c_str());
                    }
            }
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

            // BL-1473 R4 --diagnose-idle: why the settle idled kit buildings. On a COPY of
            // the handoff world every idled kit building is switched back on (streak 0) and
            // one tick runs; each kit building's estimate is then read from that tick's
            // report - revenue, input cost, maintenance, wages - with its market's price,
            // supply and demand for its output good. Buildings that never idled are the
            // control. Reported only; the census below runs on the untouched world.
            if (diagnose_idle)
            {
                world c = w;
                std::set<entity_id> was_idled;
                for (const entity_id nid : majors)
                    if (const opening_kit_major* km = fin.kit.of(nid))
                        for (const opening_kit_slot_record& r : km->slots)
                        {
                            const auto b = c.buildings.find(r.building);
                            if (b == c.buildings.end() || !b->second.decommissioned) continue;
                            b->second.decommissioned = false;
                            b->second.loss_streak    = 0;
                            was_idled.insert(r.building);
                        }
                // Three ticks, the last one read, so labour and feed can reach a building
                // that was off; a building the reflex idles again inside them reads as such.
                settle_tick_result t;
                for (int k = 0; k < 3; ++k)
                    t = run_settle_tick(c, seed_reg, k_campaign_settle_ticks + k,
                                        k_econ_tick_days_diag * (k + 1), true);
                static const char* k_cat[] = {"extraction", "processing", "civic"};
                // The labour pool's aggregate, min(1, supply / demand), per (nation, body).
                for (const lp_nation_row& n : t.report.logistic_points.nations)
                    if (major_set.count(n.nation))
                    {
                        float claim = 0.0f, granted = 0.0f;
                        for (const float v : n.claim) claim += v;
                        for (const float v : n.granted) granted += v;
                        std::printf("LPPOOL seed=%d nation=%llu pool=%.1f supply_granted=%.1f claim=%.1f granted=%.1f\n",
                                    seed, static_cast<unsigned long long>(n.nation), n.pool, n.supply_granted,
                                    claim, granted);
                    }
                for (const auto& [key, scalar] : t.report.workforce_contention)
                    if (major_set.count(key.first))
                        std::printf("CONTENTION seed=%d nation=%llu body=%llu scalar=%.3f\n", seed,
                                    static_cast<unsigned long long>(key.first),
                                    static_cast<unsigned long long>(key.second), scalar);
                for (const entity_id nid : majors)
                    if (const opening_kit_major* km = fin.kit.of(nid))
                        for (const opening_kit_slot_record& r : km->slots)
                        {
                            if (r.building == null_entity || r.category == kit_slot_category::civic) continue;
                            const auto b = c.buildings.find(r.building);
                            if (b == c.buildings.end()) continue;
                            const building_profit bp = estimate_building_profit(c, seed_reg, t.report, r.building);
                            const building_report* br = nullptr;
                            if (const auto ri = t.report.building_row.find(r.building);
                                ri != t.report.building_row.end() && ri->second < t.report.buildings.size())
                                br = &t.report.buildings[ri->second];
                            // The output good: the extracted resource, or the recipe's largest output.
                            resource_type good = b->second.type == building_type::extraction_site
                                                     ? (br ? br->target_resource : resource_type::iron_ore)
                                                     : resource_type::iron_ore;
                            if (b->second.type == building_type::processing_facility)
                                if (const recipe* rec = seed_reg.get_recipe(b->second.recipe))
                                {
                                    float best = 0.0f;
                                    for (std::size_t g = 0; g < resource_count; ++g)
                                        if (rec->outputs[g] > best) { best = rec->outputs[g]; good = static_cast<resource_type>(g); }
                                }
                            const lp_building_row* lp = nullptr;
                            for (const lp_building_row& lr : t.report.logistic_points.buildings)
                                if (lr.building == r.building) { lp = &lr; break; }
                            float price = 0.0f, supply = 0.0f, demand = 0.0f, deposit = 0.0f;
                            if (const auto tl = c.tiles.find(b->second.tile); tl != c.tiles.end())
                                deposit = tl->second.resource_remaining[static_cast<std::size_t>(good)];
                            if (const entity_id mid = market_for_tile(c, b->second.tile); mid != null_entity)
                                if (const auto m = c.markets.find(mid); m != c.markets.end())
                                {
                                    const std::size_t g = static_cast<std::size_t>(good);
                                    price = m->second.price[g]; supply = m->second.supply[g]; demand = m->second.demand[g];
                                }
                            std::printf("LOSS seed=%d nation=%llu cat=%s slot=%s good=%s idled=%d data=%d"
                                        " out=%.2f run=%.2f limiting=%s revenue=%.2f input=%.2f maint=%.2f wages=%.2f"
                                        " net=%.2f price=%.3f supply=%.1f demand=%.1f wf=%.2f eff_wf=%.2f idle=%d"
                                        " exhausted=%d deposit=%.1f off=%d state_idle=%d grant=%.3f share=%.3f"
                                        " lp_need=%.2f lp_cover=%.3f lp_frac=%.3f\n",
                                        seed, static_cast<unsigned long long>(nid), k_cat[static_cast<int>(r.category)],
                                        r.slot.c_str(), resource_names::name_of(good).c_str(), was_idled.count(r.building) ? 1 : 0,
                                        bp.has_data ? 1 : 0, br ? br->output_quantity : 0.0f, br ? br->run : 0.0f,
                                        br && br->has_limiting ? resource_names::name_of(br->limiting_input).c_str() : "-",
                                        bp.revenue, bp.input_cost, bp.maintenance, bp.wages, bp.net(), price, supply, demand,
                                        b->second.workforce_assigned, br ? br->effective_workforce : 0.0f,
                                        br && br->idle ? 1 : 0, br && br->exhausted ? 1 : 0, deposit,
                                        b->second.decommissioned ? 1 : 0, b->second.state_idle ? 1 : 0,
                                        t.report.building_labour.count(r.building)
                                            ? t.report.building_labour.at(r.building) : -1.0f,
                                        building_output_share(b->second), lp ? lp->need : -1.0f,
                                        lp ? lp->coverage : -1.0f, lp ? lp->fraction : -1.0f);
                        }
            }

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
                const std::array<int, 4>& ih = idled_at_handoff[nid];
                int ie[4] = {0, 0, 0, 0};
                if (e_it != w.nations.end()) kit_idled(nid, ie);
                std::printf("] constr %d  units %d men %ld pool %d | kit %d/%d/%d proc %d uncovered %d idled %d | play:"
                            " net mean %8.1f min %8.1f red %2d/%zu  end treasury %9.0f built %3d men %ld\n",
                            h.constructing, h.units, h.men, h.land_pool, kp, ks, ku, kpb, unc, h.idled, mean, mn, red, q.size(),
                            e.treasury, e.built_all, e.men);
                std::printf("ROW seed=%d pair=%d:%d nation=%llu tiles=%d treasury=%.1f built=%d ext=%d proc=%d logi=%d"
                            " civic=%d mil=%d other=%d constr=%d units=%d men=%ld pool=%d net_mean=%.2f net_min=%.2f"
                            " red=%d quarters=%zu end_treasury=%.1f end_built=%d end_men=%ld"
                            " kit_placed=%d kit_sub=%d kit_unbuilt=%d proc_built=%d uncovered=%d idled=%d"
                            " idled_kit_ext=%d idled_kit_proc=%d idled_kit_civic=%d idled_other=%d proc_running=%d"
                            " end_idled_kit_ext=%d end_idled_kit_proc=%d end_idled_kit_civic=%d end_idled_other=%d\n",
                            seed, tt, mt, static_cast<unsigned long long>(nid), h.tiles, h.treasury, h.built_all,
                            h.built[k_extraction], h.built[k_processing], h.built[k_logistics], h.built[k_civic],
                            h.built[k_military], h.built[k_other], h.constructing, h.units, h.men, h.land_pool,
                            mean, mn, red, q.size(), e.treasury, e.built_all, e.men, kp, ks, ku, kpb, unc, h.idled,
                            ih[0], ih[1], ih[2], ih[3], kpb - ih[1], ie[0], ie[1], ie[2], ie[3]);
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
