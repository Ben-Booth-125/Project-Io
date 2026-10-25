// enclave_trace — a PURE READER for NR-1077 (a capital on a tiny enclave).
//
// For every nation whose capital stands on a land group much smaller than the
// nation's land, it prints the group sizes, the realm the nation folds from, and
// the history of the capital's carve cell and of every cell bordering it: who
// held each, year by year, across the three recorded spans -- so a reader can
// tell how the enclave arose (the carve, Pass 2b, a cession). The traced case
// (seed 1) was a cession that left a Pass 2b islet with its cedent; § Pass 2e
// now carries it with its province (Ben, 2026-10-25). The SUMMARY line counts
// nations seated on a speck, world_audit's SPECK row.
//
// Build:  bash tools/verify/build_lua_harness.sh enclave_trace
// Run:    build_gen/verify/enclave_trace.exe [--seed N] [--target T:M] [--polity P] [--at TILE]...

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/nation_generation.hpp"
#include "world/world.hpp"
#include "world/era_minus_one.hpp"
#include "world/era_timelapse.hpp"
#include "world/history_sim.hpp"
#include "world/closing_rounds.hpp"
#include "world/works_roster.hpp"
#include "world/world_gen_config.hpp"
#include "scripting/lua_state.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <vector>

namespace {

int nb4(int t, int gw, int gh, int out[4])
{
    const int c = t % gw, r = t / gw;
    int n = 0;
    out[n++] = r * gw + (c + 1) % gw;
    out[n++] = r * gw + (c - 1 + gw) % gw;
    if (r > 0) out[n++] = t - gw;
    if (r + 1 < gh) out[n++] = t + gw;
    return n;
}

} // namespace

static int  g_polity = -1;
static std::vector<int> g_at; ///< `--at T`: raster tiles to report
static const char* g_dump = nullptr; ///< `--dump FILE`: per land tile "tile owner-name", for diffing two builds' maps

int main(int argc, char** argv)
{
    uint32_t seed = 1;
    int tt = 20, mt = 4;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        else if (std::strcmp(argv[i], "--target") == 0 && i + 1 < argc) std::sscanf(argv[++i], "%d:%d", &tt, &mt);
        else if (std::strcmp(argv[i], "--polity") == 0 && i + 1 < argc) g_polity = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--at") == 0 && i + 1 < argc) g_at.push_back(std::atoi(argv[++i]));
        else if (std::strcmp(argv[i], "--dump") == 0 && i + 1 < argc) g_dump = argv[++i];
    }
    lua_state        lua;
    world_gen_config cfg{};
    works_registry   works;
    lua.load("scripts/world_gen.lua");
    cfg.load_from_lua(lua);
    lua.load("scripts/works.lua");
    works.load_from_lua(lua);

    world_params p = arc_params(world_arc::shipped);
    p.seed                = seed;
    p.nation_total_target = tt;
    p.major_target        = mt;
    generation_report     rep{};
    era_minus_one_fixture fx{};
    world w = make_hard_coded_world(p, &rep, cfg, nullptr, &works, &fx);

    const entity_id body = w.home_body;
    const int gw = w.bodies.at(body).grid_width, gh = w.bodies.at(body).grid_height, total = gw * gh;
    std::vector<int>  own(static_cast<std::size_t>(total), -1);
    std::vector<char> water(static_cast<std::size_t>(total), 1);
    std::map<entity_id, int> nidx;
    std::vector<entity_id> nids;
    for (const auto& [nid, n] : w.nations) { nidx[nid] = static_cast<int>(nids.size()); nids.push_back(nid); }
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body) continue;
        const int t = tc.grid_y * gw + tc.grid_x;
        water[static_cast<std::size_t>(t)] = is_water(tc.substrate) ? 1 : 0;
        const auto it = w.tile_to_nation.find(tid);
        if (it != w.tile_to_nation.end() && !water[static_cast<std::size_t>(t)]) own[static_cast<std::size_t>(t)] = nidx[it->second];
    }
    // groups
    std::vector<int> comp(static_cast<std::size_t>(total), -1), csize;
    for (int s = 0; s < total; ++s)
    {
        if (own[s] < 0 || comp[s] >= 0) continue;
        const int id = static_cast<int>(csize.size());
        csize.push_back(0);
        std::vector<int> st{ s };
        comp[s] = id;
        while (!st.empty())
        {
            const int t = st.back(); st.pop_back(); ++csize[id];
            int nb[4]; const int n = nb4(t, gw, gh, nb);
            for (int i = 0; i < n; ++i) if (own[nb[i]] == own[s] && comp[nb[i]] < 0) { comp[nb[i]] = id; st.push_back(nb[i]); }
        }
    }

    const auto& hs = fx.industrialisation_handoff;
    const std::vector<int> cells = fx.industrialisation_closing
                                       ? closing_carve_cells(*fx.industrialisation_closing, hs.regions)
                                       : std::vector<int>{};
    const generation_report::body_entry* be = nullptr;
    for (const auto& b : rep.bodies) if (b.is_homeworld) be = &b;

    {
        const history_sim_state& is = fx.industrialisation_state;
        std::printf("CESSIONS closing rounds: %d (islet-only %d, to unheld %d, late %d, split %d) | islet tiles carried %d | overrides %zu\n",
                    is.closing_cessions, is.closing_cessions_islet, is.closing_cessions_to_unheld, is.closing_cessions_late,
                    is.closing_cessions_split, is.closing_islet_tiles_carried, is.closing_cell_overrides.size());
    }
    {
        const auto census = [](const char* tag, const std::vector<polity>& ps, const std::vector<region>& rs) {
            int alive = 0, held = 0, biggest = 0;
            std::map<int, int> per;
            for (const region& r : rs) if (r.nation >= 0) { ++per[r.nation]; ++held; }
            for (const polity& p : ps) alive += p.alive ? 1 : 0;
            for (const auto& [pid, n] : per) biggest = std::max(biggest, n);
            std::printf("REALMS %s: polities %zu, alive %d, holding %zu, regions %zu (held %d), largest realm %d regions\n",
                        tag, ps.size(), alive, per.size(), rs.size(), held, biggest);
        };
        census("1660", fx.exploration_handoff.polities, fx.exploration_handoff.regions);
        census("1960", fx.industrialisation_handoff.polities, fx.industrialisation_handoff.regions);
    }
    if (g_polity >= 0 && be)
    {
        const auto held_at = [&](const std::vector<region>& rs) {
            int n = 0;
            for (const region& r : rs) n += r.nation == g_polity ? 1 : 0;
            return n;
        };
        const auto& ps = fx.industrialisation_handoff.polities;
        std::printf("POLITY %d name=%s: regions held 1660 %d, 1960 %d, alive at 1960 %d\n", g_polity,
                    g_polity < static_cast<int>(ps.size()) ? ps[g_polity].name.c_str() : "-",
                    held_at(fx.exploration_handoff.regions), held_at(fx.industrialisation_handoff.regions),
                    g_polity < static_cast<int>(ps.size()) && ps[g_polity].alive ? 1 : 0);
        const char* span[3] = { "empires", "exploration", "industrialisation" };
        int k = 0;
        for (const era_timelapse* tl : { &be->prehistory_timelapse, &be->exploration_timelapse, &be->industrialisation_timelapse })
        {
            for (const lapse_event& e : tl->events)
                if ((e.kind == static_cast<uint8_t>(lapse_event_kind::realm_ended) || e.kind == static_cast<uint8_t>(lapse_event_kind::founded)
                     || e.kind == static_cast<uint8_t>(lapse_event_kind::broke_away) || e.kind == static_cast<uint8_t>(lapse_event_kind::exclave_ceded)
                     || e.kind == static_cast<uint8_t>(lapse_event_kind::absorbed_at_close))
                    && (e.polity == g_polity || e.other == g_polity))
                    std::printf("   %s year %d kind %d region %u polity %u other %u\n", span[k], e.year, e.kind, e.region, e.polity, e.other);
            ++k;
        }
        for (const auto& [nid, n] : w.nations)
            if (n.name == (g_polity < static_cast<int>(ps.size()) ? ps[g_polity].name : std::string("?")))
                std::printf("   nation %u carries its name: %zu tiles\n", static_cast<unsigned>(nid), n.tiles.size());
    }
    std::printf("NATIONS %zu\n", nids.size());
    int enclaves = 0, specks = 0;

    for (std::size_t ni = 0; ni < nids.size(); ++ni)
    {
        const nation_component& n = w.nations.at(nids[ni]);
        const auto tit = w.tiles.find(n.capital_tile);
        if (tit == w.tiles.end()) continue;
        const int ct = tit->second.grid_y * gw + tit->second.grid_x;
        int land = 0;
        for (int t = 0; t < total; ++t) land += own[t] == static_cast<int>(ni) ? 1 : 0;
        const int cg = comp[ct] >= 0 ? csize[comp[ct]] : 0;
        // NR-1077's class (world_audit's SPECK row): a capital on a group under
        // 10 tiles while a larger group of the nation's own lies elsewhere.
        int largest_other = 0;
        {
            std::map<int, int> sz;
            for (int t = 0; t < total; ++t)
                if (own[t] == static_cast<int>(ni) && comp[t] != comp[ct]) ++sz[comp[t]];
            for (const auto& [g, s] : sz) largest_other = std::max(largest_other, s);
        }
        const bool speck = land > 0 && cg < 10 && largest_other > cg;
        specks += speck ? 1 : 0;
        if (land <= 0 || (cg * 10 >= land && !speck)) continue;
        enclaves += cg * 10 < land ? 1 : 0;
        int polity = -1;
        if (be) for (std::size_t i = 0; i < be->nation_ids.size() && i < be->nation_polity.size(); ++i)
            if (be->nation_ids[i] == nids[ni]) polity = be->nation_polity[i];
        std::printf("ENCLAVE nation=%u name=%s polity=%d land=%d capital_tile=%d capital_group=%d (col %d row %d)\n",
                    static_cast<unsigned>(nids[ni]), n.name.c_str(), polity, land, ct, cg, ct % gw, ct / gw);
        std::map<int, int> gsz;
        for (int t = 0; t < total; ++t) if (own[t] == static_cast<int>(ni)) ++gsz[comp[t]];
        for (const auto& [g, s] : gsz)
        {
            int c0 = gw, c1 = -1, r0 = gh, r1 = -1;
            std::map<int, int> by_cell;
            for (int t = 0; t < total; ++t)
                if (comp[t] == g)
                {
                    c0 = std::min(c0, t % gw); c1 = std::max(c1, t % gw);
                    r0 = std::min(r0, t / gw); r1 = std::max(r1, t / gw);
                    if (!cells.empty()) ++by_cell[cells[t]];
                }
            std::printf("   group %d: %d tiles, cols %d..%d rows %d..%d; oracle cells:", g, s, c0, c1, r0, r1);
            for (const auto& [c, k] : by_cell) std::printf(" %d(%d)", c, k);
            std::printf("\n");
        }
        // capital group's neighbours (who surrounds it)
        std::map<int, int> around;
        for (int t = 0; t < total; ++t)
        {
            if (comp[t] != comp[ct]) continue;
            int nb[4]; const int k = nb4(t, gw, gh, nb);
            for (int i = 0; i < k; ++i) if (own[nb[i]] >= 0 && own[nb[i]] != static_cast<int>(ni)) ++around[own[nb[i]]];
                                        else if (water[nb[i]]) ++around[-2];
        }
        for (const auto& [o, c] : around)
            std::printf("   borders %s %s: %d edges\n", o == -2 ? "WATER" : "nation",
                        o >= 0 ? w.nations.at(nids[o]).name.c_str() : "", c);
        if (cells.empty() || polity < 0 || polity >= static_cast<int>(hs.polities.size())) continue;
        const int seat = hs.polities[polity].capital;
        std::printf("   realm %d seat region %d (%s), alive=%d\n", polity, seat,
                    seat >= 0 ? hs.regions[seat].name.c_str() : "-", hs.polities[polity].alive ? 1 : 0);
        // the realm's regions at the close, by landmass (carve cell)
        std::set<int> watch;
        if (seat >= 0) watch.insert(seat);
        // cells bordering the seat's cell
        for (int t = 0; t < total; ++t)
        {
            if (cells[t] != seat) continue;
            int nb[4]; const int k = nb4(t, gw, gh, nb);
            for (int i = 0; i < k; ++i) if (cells[nb[i]] >= 0 && cells[nb[i]] != seat && !water[nb[i]]) watch.insert(cells[nb[i]]);
        }
        int seat_cell_tiles = 0, seat_cell_land = 0;
        for (int t = 0; t < total; ++t) if (cells[t] == seat) { ++seat_cell_tiles; seat_cell_land += water[t] ? 0 : 1; }
        std::printf("   seat cell: %d tiles (%d land); final nation of the capital tile's cell-holder\n", seat_cell_tiles, seat_cell_land);
        // ownership history per watched region
        for (const int r : watch)
        {
            std::printf("   region %d %-16s final owner %d  seat_region %d is_seat %d | history:", r, hs.regions[r].name.c_str(),
                        hs.regions[r].nation, hs.regions[r].seat_region,
                        hs.regions[r].is_seat ? 1 : 0);
            if (be)
                for (const era_timelapse* tl : { &be->prehistory_timelapse, &be->exploration_timelapse, &be->industrialisation_timelapse })
                {
                    std::printf(" ||");
                    for (const owner_change& oc : tl->changes) if (oc.region == r) std::printf(" %d:%d", oc.year, oc.owner == 0xFFFF ? -1 : oc.owner);
                }
            std::printf("\n");
        }
        // every region the realm ever held, with its whole recorded history
        if (be)
        {
            std::set<int> ever;
            for (const era_timelapse* tl : { &be->prehistory_timelapse, &be->exploration_timelapse, &be->industrialisation_timelapse })
                for (const owner_change& oc : tl->changes) if (oc.owner == polity) ever.insert(oc.region);
            for (const int r : ever)
            {
                if (watch.count(r)) continue;
                int land_r = 0;
                for (int t = 0; t < total; ++t) land_r += (cells[t] == r && !water[t]) ? 1 : 0;
                std::printf("   held %d (%d land tiles, col %d row %d) final %d:", r, land_r, hs.regions[r].col, hs.regions[r].row, hs.regions[r].nation);
                int shown = 0;
                for (const era_timelapse* tl : { &be->prehistory_timelapse, &be->exploration_timelapse, &be->industrialisation_timelapse })
                {
                    std::printf(" ||");
                    for (const owner_change& oc : tl->changes)
                        if (oc.region == r && shown++ < 24) std::printf(" %d:%d", oc.year, oc.owner == 0xFFFF ? -1 : oc.owner);
                }
                std::printf("\n");
            }
        }
        // the realm's regions at the close
        std::printf("   realm %d holds at close:", polity);
        for (std::size_t r = 0; r < hs.regions.size(); ++r) if (hs.regions[r].nation == polity) std::printf(" %zu", r);
        std::printf("\n");
        // cession events in the industrialisation record naming this realm
        if (be)
            for (const lapse_event& e : be->industrialisation_timelapse.events)
                if (e.kind == static_cast<uint8_t>(lapse_event_kind::exclave_ceded) && (e.polity == polity || e.other == polity))
                    std::printf("   CESSION year %d region %u from %u to %u\n", e.year, e.region, e.polity, e.other);
    }
    if (g_dump != nullptr)
        if (FILE* f = std::fopen(g_dump, "w"))
        {
            for (int t = 0; t < total; ++t)
                if (!water[static_cast<std::size_t>(t)])
                    std::fprintf(f, "%d %s\n", t,
                                 own[static_cast<std::size_t>(t)] >= 0
                                     ? w.nations.at(nids[static_cast<std::size_t>(own[static_cast<std::size_t>(t)])]).name.c_str()
                                     : "-");
            std::fclose(f);
        }
    // --at T: who holds raster tile T, its group, and its oracle cell's region.
    for (const int t : g_at)
    {
        if (t < 0 || t >= total) continue;
        const int o = own[static_cast<std::size_t>(t)];
        int polity = -1;
        if (o >= 0 && be)
            for (std::size_t i = 0; i < be->nation_ids.size() && i < be->nation_polity.size(); ++i)
                if (be->nation_ids[i] == nids[static_cast<std::size_t>(o)]) polity = be->nation_polity[i];
        std::printf("AT tile %d (col %d row %d): %s nation=%s polity=%d group=%d tiles, oracle cell %d (owner at close %d)\n", t, t % gw,
                    t / gw, water[static_cast<std::size_t>(t)] ? "water" : "land",
                    o >= 0 ? w.nations.at(nids[static_cast<std::size_t>(o)]).name.c_str() : "-", polity,
                    comp[static_cast<std::size_t>(t)] >= 0 ? csize[static_cast<std::size_t>(comp[static_cast<std::size_t>(t)])] : 0,
                    cells.empty() ? -1 : cells[static_cast<std::size_t>(t)],
                    (cells.empty() || cells[static_cast<std::size_t>(t)] < 0) ? -1
                        : hs.regions[static_cast<std::size_t>(cells[static_cast<std::size_t>(t)])].nation);
    }
    std::printf("SUMMARY seed %u: %d nation(s) seated on a group under a tenth of their land, %d on a speck (<10 tiles, a larger group elsewhere)\n",
                seed, enclaves, specks);
    return 0;
}
