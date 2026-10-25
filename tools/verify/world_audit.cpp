// Throwaway headless harness: builds the hard-coded world and reports
//   (S2) Kepler's forest + wetland tile fraction (biome-balance target ~>=3%
//        of LAND — re-based from "of all tiles" 2026-08-09, BL-338. Forest and
//        wetland are land-only biomes, so counting Kepler's 9,028 ocean tiles in
//        the denominator asked the wrong question: the same world reads 2.4% or
//        6.0% depending only on how wet the planet is. The threshold is
//        deliberately unchanged at 3% — the point of the re-base is to make the
//        number mean something, not to make it easier to clear.);
//   (S1) whether every placed extraction asset sits on a tile carrying a
//        non-zero prototype-extractable deposit (iron ore / petroleum / water /
//        agricultural produce) on valid (non-ocean) terrain.
// No SDL / Lua / ImGui. Kept outside src/ so the CMake glob ignores it.

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/nation_generation.hpp"
#include "world/placement_rules.hpp"
#include "world/population_generation.hpp"
#include "world/terrain_combat.hpp"
#include "world/world.hpp"
#include "world/era_minus_one.hpp"
#include "world/finish_campaign_world.hpp"
#include "world/recipe_registry.hpp"
#include "world/era_timelapse.hpp"
#include "world/history_sim.hpp"
#include "world/works_roster.hpp"
#include "world/world_gen_config.hpp"
#include "scripting/lua_state.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

static std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

// ---------------------------------------------------------------------------
// BL-284 — territorial fragmentation, measured and ATTRIBUTED
// ---------------------------------------------------------------------------
//
// BL-218 bought the expensive settlement-sim path largely on the argument that
// fragmentation would fall out of it for free: a growth front that crosses a
// strait and stalls, or a nation cut in half by a rival's expansion, leaves an
// exclave nobody authored. Nothing counted them, so the argument was never
// tested. This block counts each nation's non-contiguous territorial components
// and attributes every exclave to one of two producers.
//
// The attribution rule, and why it is exact rather than a heuristic:
//
//   * Pass 2 (the Voronoi BFS) is WATER-BLOCKED. It can therefore only claim
//     tiles on a landmass that already contains a seed — and on such a landmass
//     it runs to exhaustion, so every land tile there ends up claimed by it.
//   * Pass 2b (orphan-island assignment) consequently only ever fires on a
//     landmass containing NO seed, and when it does it hands the whole landmass
//     to one nation in a single act.
//
// So: an exclave sitting on a SEEDED landmass (one holding at least one region
// anchor) is emergent — the sim itself produced it, either by a front stalling
// or by a rival cutting it off. An
// exclave on a SEEDLESS landmass was authored by Pass 2b's cleanup and is not
// evidence for BL-218's argument.
struct frag_stats
{
    int nations              = 0;
    int nations_with_exclave = 0;
    int exclaves_emergent    = 0;
    int exclaves_pass2b      = 0;
    int tiles_emergent       = 0;
    int tiles_pass2b         = 0;
    int landmasses           = 0;
    int landmasses_seedless  = 0;
};

static frag_stats measure_fragmentation(const world& w, const generation_report& rep)
{
    frag_stats fs;

    const entity_id body = w.home_body;
    const auto      bit  = w.bodies.find(body);
    if (bit == w.bodies.end())
        return fs;
    const int gw    = bit->second.grid_width;
    const int gh    = bit->second.grid_height;
    const int total = gw * gh;
    if (total <= 0)
        return fs;

    // Raster views of the home body: land flag and owning-nation entity id.
    std::vector<char>      is_land(static_cast<std::size_t>(total), 0);
    std::vector<entity_id> owner(static_cast<std::size_t>(total), null_entity);
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body) continue;
        if (is_water(tc.substrate)) continue;
        const int idx = tc.grid_y * gw + tc.grid_x;
        if (idx < 0 || idx >= total) continue;
        is_land[static_cast<std::size_t>(idx)] = 1;
        const auto nit = w.tile_to_nation.find(tid);
        if (nit != w.tile_to_nation.end())
            owner[static_cast<std::size_t>(idx)] = nit->second;
    }

    // Cardinal neighbours, column-wrapped, rows not wrapped — the same adjacency
    // the BFS and the orphan pass use, so the components line up with theirs.
    auto neighbours = [gw, gh](int idx, int out[4]) {
        const int col = idx % gw;
        const int row = idx / gw;
        int n = 0;
        out[n++] = row * gw + ((col + 1) % gw);
        out[n++] = row * gw + ((col - 1 + gw) % gw);
        if (row > 0)      out[n++] = (row - 1) * gw + col;
        if (row < gh - 1) out[n++] = (row + 1) * gw + col;
        return n;
    };

    // --- landmasses (ownership-blind), and which of them the sim seeded -------
    std::vector<int> landmass(static_cast<std::size_t>(total), -1);
    int              lm_count = 0;
    for (int start = 0; start < total; ++start)
    {
        if (!is_land[static_cast<std::size_t>(start)]) continue;
        if (landmass[static_cast<std::size_t>(start)] >= 0) continue;
        const int id = lm_count++;
        std::vector<int> stack{ start };
        landmass[static_cast<std::size_t>(start)] = id;
        while (!stack.empty())
        {
            const int idx = stack.back();
            stack.pop_back();
            int nb[4];
            const int n = neighbours(idx, nb);
            for (int i = 0; i < n; ++i)
            {
                if (!is_land[static_cast<std::size_t>(nb[i])]) continue;
                if (landmass[static_cast<std::size_t>(nb[i])] >= 0) continue;
                landmass[static_cast<std::size_t>(nb[i])] = id;
                stack.push_back(nb[i]);
            }
        }
    }
    fs.landmasses = lm_count;

    std::vector<char> lm_seeded(static_cast<std::size_t>(lm_count), 0);
    const generation_report::body_entry* home_entry = nullptr;
    for (const auto& be : rep.bodies)
        if (be.name == bit->second.name) { home_entry = &be; break; }
    if (home_entry)
    {
        for (const region& p : home_entry->settlement.regions)
        {
            if (p.anchor < 0 || p.anchor >= total) continue;
            const int lm = landmass[static_cast<std::size_t>(p.anchor)];
            if (lm >= 0) lm_seeded[static_cast<std::size_t>(lm)] = 1;
        }
    }
    for (int i = 0; i < lm_count; ++i)
        if (!lm_seeded[static_cast<std::size_t>(i)]) ++fs.landmasses_seedless;

    // --- per-nation territorial components -----------------------------------
    std::vector<int> comp(static_cast<std::size_t>(total), -1);
    struct comp_rec { entity_id nation; int size; int landmass; };
    std::vector<comp_rec> comps;
    for (int start = 0; start < total; ++start)
    {
        if (owner[static_cast<std::size_t>(start)] == null_entity) continue;
        if (comp[static_cast<std::size_t>(start)] >= 0) continue;
        const entity_id nid = owner[static_cast<std::size_t>(start)];
        const int       id  = static_cast<int>(comps.size());
        comps.push_back({ nid, 0, landmass[static_cast<std::size_t>(start)] });
        std::vector<int> stack{ start };
        comp[static_cast<std::size_t>(start)] = id;
        while (!stack.empty())
        {
            const int idx = stack.back();
            stack.pop_back();
            ++comps[static_cast<std::size_t>(id)].size;
            int nb[4];
            const int n = neighbours(idx, nb);
            for (int i = 0; i < n; ++i)
            {
                if (owner[static_cast<std::size_t>(nb[i])] != nid) continue;
                if (comp[static_cast<std::size_t>(nb[i])] >= 0) continue;
                comp[static_cast<std::size_t>(nb[i])] = id;
                stack.push_back(nb[i]);
            }
        }
    }

    // The largest component is the mainland; every other one is an exclave.
    std::map<entity_id, std::vector<int>> by_nation;
    for (int i = 0; i < static_cast<int>(comps.size()); ++i)
        by_nation[comps[static_cast<std::size_t>(i)].nation].push_back(i);

    fs.nations = static_cast<int>(by_nation.size());
    for (const auto& [nid, ids] : by_nation)
    {
        (void)nid;
        int mainland = ids.front();
        for (int i : ids)
            if (comps[static_cast<std::size_t>(i)].size
                > comps[static_cast<std::size_t>(mainland)].size)
                mainland = i;
        if (ids.size() > 1) ++fs.nations_with_exclave;
        for (int i : ids)
        {
            if (i == mainland) continue;
            const comp_rec& c = comps[static_cast<std::size_t>(i)];
            const bool seeded = c.landmass >= 0
                             && lm_seeded[static_cast<std::size_t>(c.landmass)] != 0;
            if (seeded) { ++fs.exclaves_emergent; fs.tiles_emergent += c.size; }
            else        { ++fs.exclaves_pass2b;   fs.tiles_pass2b   += c.size; }
        }
    }

    return fs;
}

// ---------------------------------------------------------------------------
// BL-1310 — ONE TERRITORY: THE STRAIT RULE, AUDITED (NATION_GENERATION.md
// § Pass 2e). Independent of the generator's own cession code on purpose: it
// re-derives every nation's land groups, its core (the group holding its
// capital tile), the groups joined to the core by straits of at most
// `strait_tiles` water tiles (chained, water-only legs, column-wrapped), and
// classifies every other group as the REMOTE ISLAND (no land neighbour of
// another nation and no other nation's ground within strait reach — the one
// exemption) or an ILLEGAL exclave. The rule's promise is zero illegal.
// ---------------------------------------------------------------------------
struct strait_stats
{
    int nations          = 0;
    int groups           = 0; ///< land groups over all nations
    int strait_joined    = 0; ///< non-core groups joined to their core by straits
    int remote           = 0; ///< exempt remote-island groups
    int remote_tiles     = 0;
    int illegal          = 0; ///< exclaves outside the rule
    int illegal_tiles    = 0;
    int illegal_anchored = 0; ///< of those, holding a region anchor (a seeded province)
    int illegal_bordered = 0; ///< of those, with a land neighbour of another nation
    int capital_off      = 0; ///< nations whose capital tile is not on their own land
};

static strait_stats audit_strait_rule(const world& w, const generation_report& rep, int strait_tiles = 3)
{
    strait_stats ss;
    const entity_id body = w.home_body;
    const auto      bit  = w.bodies.find(body);
    if (bit == w.bodies.end()) return ss;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height, total = gw * gh;
    if (total <= 0) return ss;

    std::vector<char>      land(static_cast<std::size_t>(total), 0), water(static_cast<std::size_t>(total), 0);
    std::vector<entity_id> owner(static_cast<std::size_t>(total), null_entity);
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body) continue;
        const int idx = tc.grid_y * gw + tc.grid_x;
        if (idx < 0 || idx >= total) continue;
        if (is_water(tc.substrate)) { water[static_cast<std::size_t>(idx)] = 1; continue; }
        land[static_cast<std::size_t>(idx)] = 1;
        const auto nit = w.tile_to_nation.find(tid);
        if (nit != w.tile_to_nation.end()) owner[static_cast<std::size_t>(idx)] = nit->second;
    }
    auto nb4 = [gw, gh](int idx, int out[4]) {
        const int col = idx % gw, row = idx / gw;
        int n = 0;
        out[n++] = row * gw + ((col + 1) % gw);
        out[n++] = row * gw + ((col - 1 + gw) % gw);
        if (row > 0) out[n++] = (row - 1) * gw + col;
        if (row < gh - 1) out[n++] = (row + 1) * gw + col;
        return n;
    };

    // Land groups per nation.
    std::vector<int> comp(static_cast<std::size_t>(total), -1);
    std::vector<entity_id> comp_owner;
    std::vector<std::vector<int>> comp_tiles;
    for (int s = 0; s < total; ++s)
    {
        const entity_id o = owner[static_cast<std::size_t>(s)];
        if (o == null_entity || comp[static_cast<std::size_t>(s)] >= 0) continue;
        const int id = static_cast<int>(comp_tiles.size());
        comp_owner.push_back(o);
        comp_tiles.emplace_back();
        std::vector<int> st{ s };
        comp[static_cast<std::size_t>(s)] = id;
        while (!st.empty())
        {
            const int t = st.back();
            st.pop_back();
            comp_tiles[static_cast<std::size_t>(id)].push_back(t);
            int nb[4];
            const int n = nb4(t, nb);
            for (int i = 0; i < n; ++i)
                if (owner[static_cast<std::size_t>(nb[i])] == o && comp[static_cast<std::size_t>(nb[i])] < 0)
                {
                    comp[static_cast<std::size_t>(nb[i])] = id;
                    st.push_back(nb[i]);
                }
        }
    }
    ss.groups = static_cast<int>(comp_tiles.size());

    std::vector<char> anchored(comp_tiles.size(), 0);
    for (const auto& be : rep.bodies)
        if (be.id == body)
            for (const region& p : be.settlement.regions)
                if (p.anchor >= 0 && p.anchor < total && comp[static_cast<std::size_t>(p.anchor)] >= 0)
                    anchored[static_cast<std::size_t>(comp[static_cast<std::size_t>(p.anchor)])] = 1;

    std::map<entity_id, std::vector<int>> by_nation;
    for (int c = 0; c < static_cast<int>(comp_tiles.size()); ++c) by_nation[comp_owner[static_cast<std::size_t>(c)]].push_back(c);
    ss.nations = static_cast<int>(by_nation.size());

    std::vector<char> in_terr(comp_tiles.size(), 0);
    std::vector<int>  wdist(static_cast<std::size_t>(total), 1 << 20);
    for (const auto& [nid, cs] : by_nation)
    {
        // The core: the group holding the capital tile, else the largest.
        int core = -1;
        const auto nit = w.nations.find(nid);
        if (nit != w.nations.end() && nit->second.capital_tile != null_entity)
        {
            const auto tit = w.tiles.find(nit->second.capital_tile);
            if (tit != w.tiles.end() && tit->second.body == body)
            {
                const int ci = tit->second.grid_y * gw + tit->second.grid_x;
                if (ci >= 0 && ci < total && owner[static_cast<std::size_t>(ci)] == nid)
                    core = comp[static_cast<std::size_t>(ci)];
            }
        }
        if (core < 0)
        {
            ++ss.capital_off;
            for (const int c : cs)
                if (core < 0 || comp_tiles[static_cast<std::size_t>(c)].size() > comp_tiles[static_cast<std::size_t>(core)].size())
                    core = c;
        }
        // Chained straits: water legs of at most `strait_tiles` from any
        // joined group's land; a leg re-starts at every group it reaches.
        std::vector<int> touched, q;
        auto join = [&](int c) {
            in_terr[static_cast<std::size_t>(c)] = 1;
            for (const int t : comp_tiles[static_cast<std::size_t>(c)]) q.push_back(t);
        };
        join(core);
        std::size_t head = 0;
        while (head < q.size())
        {
            const int t = q[head++];
            const int d = land[static_cast<std::size_t>(t)] ? 0 : wdist[static_cast<std::size_t>(t)];
            int nb[4];
            const int n = nb4(t, nb);
            for (int i = 0; i < n; ++i)
            {
                const int u = nb[i];
                if (water[static_cast<std::size_t>(u)])
                {
                    if (d + 1 > strait_tiles || wdist[static_cast<std::size_t>(u)] <= d + 1) continue;
                    if (wdist[static_cast<std::size_t>(u)] == (1 << 20)) touched.push_back(u);
                    wdist[static_cast<std::size_t>(u)] = d + 1;
                    q.push_back(u);
                }
                else if (owner[static_cast<std::size_t>(u)] == nid && !in_terr[static_cast<std::size_t>(comp[static_cast<std::size_t>(u)])])
                    join(comp[static_cast<std::size_t>(u)]);
            }
        }
        for (const int t : touched) wdist[static_cast<std::size_t>(t)] = 1 << 20;
        for (const int c : cs)
            if (c != core && in_terr[static_cast<std::size_t>(c)]) ++ss.strait_joined;
    }

    // Everything outside a territory: the remote island, or illegal.
    for (int c = 0; c < static_cast<int>(comp_tiles.size()); ++c)
    {
        if (in_terr[static_cast<std::size_t>(c)]) continue;
        const entity_id o = comp_owner[static_cast<std::size_t>(c)];
        bool bordered = false, reach_other = false;
        std::vector<int> touched, q;
        for (const int t : comp_tiles[static_cast<std::size_t>(c)]) q.push_back(t);
        std::size_t head = 0;
        while (head < q.size() && !reach_other)
        {
            const int t = q[head++];
            const int d = land[static_cast<std::size_t>(t)] ? 0 : wdist[static_cast<std::size_t>(t)];
            int nb[4];
            const int n = nb4(t, nb);
            for (int i = 0; i < n; ++i)
            {
                const int u = nb[i];
                if (water[static_cast<std::size_t>(u)])
                {
                    if (d + 1 > strait_tiles || wdist[static_cast<std::size_t>(u)] <= d + 1) continue;
                    if (wdist[static_cast<std::size_t>(u)] == (1 << 20)) touched.push_back(u);
                    wdist[static_cast<std::size_t>(u)] = d + 1;
                    q.push_back(u);
                }
                else if (owner[static_cast<std::size_t>(u)] != null_entity && owner[static_cast<std::size_t>(u)] != o)
                {
                    reach_other = true;
                    if (d == 0) bordered = true;
                }
            }
        }
        for (const int t : touched) wdist[static_cast<std::size_t>(t)] = 1 << 20;
        const int sz = static_cast<int>(comp_tiles[static_cast<std::size_t>(c)].size());
        if (!reach_other) { ++ss.remote; ss.remote_tiles += sz; continue; }
        ++ss.illegal;
        ss.illegal_tiles += sz;
        ss.illegal_anchored += anchored[static_cast<std::size_t>(c)] ? 1 : 0;
        if (!bordered)
        {
            // The early exit above may stop before a land edge is seen: look directly.
            for (const int t : comp_tiles[static_cast<std::size_t>(c)])
            {
                int nb[4];
                const int n = nb4(t, nb);
                for (int i = 0; i < n; ++i)
                    if (owner[static_cast<std::size_t>(nb[i])] != null_entity && owner[static_cast<std::size_t>(nb[i])] != o)
                        bordered = true;
            }
        }
        ss.illegal_bordered += bordered ? 1 : 0;
    }
    return ss;
}

static void print_strait(const char* tag, uint32_t seed, const strait_stats& s, int cessions)
{
    std::printf("  %s seed %u: %d nations, %d groups | strait-joined %d | remote %d (%d tiles) | "
                "ILLEGAL %d (%d tiles; %d anchored, %d land-bordered) | capital off-ground %d | cessions %d\n",
                tag, seed, s.nations, s.groups, s.strait_joined, s.remote, s.remote_tiles, s.illegal,
                s.illegal_tiles, s.illegal_anchored, s.illegal_bordered, s.capital_off, cessions);
}

static int count_cessions(const generation_report& rep)
{
    int n = 0;
    for (const auto& be : rep.bodies)
        if (be.is_homeworld)
            for (const era_timelapse* t : { &be.prehistory_timelapse, &be.exploration_timelapse,
                                            &be.industrialisation_timelapse })
                for (const lapse_event& e : t->events)
                    if (e.kind == static_cast<uint8_t>(lapse_event_kind::exclave_ceded)) ++n;
    return n;
}

// NR-1077 (Ben, 2026-10-25) — A CAPITAL ON A SPECK. Per nation: the land
// group (cardinal, column-wrapped, same owner) holding its capital tile, and
// its largest other group holding a province. A nation whose capital group is
// under `k_speck_tiles` while such a group is larger is seated on a speck with
// its main ground elsewhere -- § Pass 2e (NR-1080, Ben 2026-10-25) seats it on
// its largest group, so the row fails on any nation, major or minor.
static constexpr int k_speck_tiles = 10;
struct capital_speck_stats
{
    int         all      = 0; ///< nations of any rank seated on a speck (the row fails on these)
    int         majors   = 0; ///< of those, majors
    int         seedless = 0; ///< reading only: on a speck beside a larger group that holds no province
    std::string detail;     ///< " | <name> capital group G of L land" per speck
};

static capital_speck_stats audit_capital_specks(const world& w, const generation_report& rep, const std::set<entity_id>& majors)
{
    capital_speck_stats cs;
    const entity_id body = w.home_body;
    const auto      bit  = w.bodies.find(body);
    if (bit == w.bodies.end()) return cs;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height, total = gw * gh;
    if (total <= 0) return cs;
    std::vector<entity_id> owner(static_cast<std::size_t>(total), null_entity);
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body || is_water(tc.substrate)) continue;
        const int idx = tc.grid_y * gw + tc.grid_x;
        if (idx < 0 || idx >= total) continue;
        const auto nit = w.tile_to_nation.find(tid);
        if (nit != w.tile_to_nation.end()) owner[static_cast<std::size_t>(idx)] = nit->second;
    }
    // Land groups of every nation, labelled once.
    std::vector<int> comp(static_cast<std::size_t>(total), -1), csize;
    std::vector<entity_id> cown;
    for (int s = 0; s < total; ++s)
    {
        const entity_id o = owner[static_cast<std::size_t>(s)];
        if (o == null_entity || comp[static_cast<std::size_t>(s)] >= 0) continue;
        const int id = static_cast<int>(csize.size());
        csize.push_back(0);
        cown.push_back(o);
        std::vector<int> st{ s };
        comp[static_cast<std::size_t>(s)] = id;
        while (!st.empty())
        {
            const int t = st.back();
            st.pop_back();
            ++csize[static_cast<std::size_t>(id)];
            const int col = t % gw, row = t / gw;
            const int nb[4] = { row * gw + (col + 1) % gw, row * gw + (col - 1 + gw) % gw,
                                row > 0 ? t - gw : -1, row + 1 < gh ? t + gw : -1 };
            for (const int u : nb)
                if (u >= 0 && comp[static_cast<std::size_t>(u)] < 0 && owner[static_cast<std::size_t>(u)] == o)
                {
                    comp[static_cast<std::size_t>(u)] = id;
                    st.push_back(u);
                }
        }
    }
    std::vector<char> anchored(csize.size(), 0);
    for (const auto& be : rep.bodies)
        if (be.id == body)
            for (const region& p : be.settlement.regions)
                if (p.anchor >= 0 && p.anchor < total && comp[static_cast<std::size_t>(p.anchor)] >= 0)
                    anchored[static_cast<std::size_t>(comp[static_cast<std::size_t>(p.anchor)])] = 1;
    std::map<entity_id, int> land;
    for (const entity_id o : owner)
        if (o != null_entity) ++land[o];
    for (const auto& [nid, n] : w.nations) // std::map: ascending ids
    {
        const auto tit = w.tiles.find(n.capital_tile);
        if (tit == w.tiles.end() || tit->second.body != body) continue;
        const int ct = tit->second.grid_y * gw + tit->second.grid_x;
        if (ct < 0 || ct >= total || owner[static_cast<std::size_t>(ct)] != nid) continue; // capital_off: another row
        const int cc    = comp[static_cast<std::size_t>(ct)];
        const int group = csize[static_cast<std::size_t>(cc)];
        // "Its main ground elsewhere" (NR-1080, Ben 2026-10-25: "seat it on
        // the nation's largest group"): a group of its own larger than the
        // capital's that holds a province -- a region anchor -- so a capital
        // could stand there. A larger group with no province (a seedless
        // landmass Pass 2b hung on the seat's own cell) is counted apart: no
        // province stands on it to seat. A nation whose every group is a
        // speck (a city state on a chain of one-tile islets) is seated on its
        // ground, not off it.
        int largest_other = 0, largest_seedless = 0;
        for (std::size_t c = 0; c < csize.size(); ++c)
            if (cown[c] == nid && static_cast<int>(c) != cc)
            {
                if (anchored[c]) largest_other = std::max(largest_other, csize[c]);
                else             largest_seedless = std::max(largest_seedless, csize[c]);
            }
        const int all_land = land[nid];
        if (group >= k_speck_tiles) continue;
        const bool major = majors.count(nid) > 0;
        char buf[200];
        if (largest_other > group)
        {
            ++cs.all;
            cs.majors += major ? 1 : 0;
            std::snprintf(buf, sizeof(buf), " | %s%s capital group %d of %d land (a province group of %d)", n.name.c_str(),
                          major ? " (MAJOR)" : "", group, all_land, largest_other);
            cs.detail += buf;
        }
        else if (largest_seedless > group)
        {
            ++cs.seedless;
            std::snprintf(buf, sizeof(buf), " | seedless: %s%s capital group %d, a provinceless group of %d", n.name.c_str(),
                          major ? " (MAJOR)" : "", group, largest_seedless);
            cs.detail += buf;
        }
    }
    return cs;
}

// THE RANDOM-PLACEMENT PATH (BL-1310 review). A `no_prehistory` world holds
// no realm and runs no closing rounds (measured: every nation's polity -1),
// so it is a no-history world on the SETTLEMENT anchors. This carves the same
// ground again through `generate_nations` with default params -- no seed
// tiles, no polities: Pass 1's random placement, Pass 2e as the post-pass
// after the fold, the size floor at its real minimum, and the repair after it
// -- and audits the strait rule on that too.
static strait_stats audit_no_history(world sw, uint32_t seed, int& nations_out)
{
    nations_out = 0;
    const entity_id body = sw.home_body;
    const auto      bit  = sw.bodies.find(body);
    if (bit == sw.bodies.end()) return {};
    const int gw = bit->second.grid_width, gh = bit->second.grid_height, total = gw * gh;
    if (total <= 0) return {};
    std::vector<entity_id> tile_ids(static_cast<std::size_t>(total), null_entity);
    for (const auto& [tid, tc] : sw.tiles)
    {
        if (tc.body != body) continue;
        const int idx = tc.grid_y * gw + tc.grid_x;
        if (idx >= 0 && idx < total) tile_ids[static_cast<std::size_t>(idx)] = tid;
    }
    sw.nations.clear();
    sw.tile_to_nation.clear();
    const nation_params np{}; // no seed tiles, no polities: the no-history path
    nations_out = static_cast<int>(generate_nations(sw, body, tile_ids, gw, gh, np, seed ^ 0x5EED1310u).size());
    const generation_report none{};
    return audit_strait_rule(sw, none);
}

static bool g_dump_capitals = false; ///< `--dump-capitals`: print CAP lines (library mode)

static std::vector<uint32_t> parse_seed_list(const char* s)
{
    std::vector<uint32_t> out;
    const char* p = s;
    while (*p)
    {
        char* end = nullptr;
        const unsigned long v = std::strtoul(p, &end, 10);
        if (end == p) break;
        out.push_back(static_cast<uint32_t>(v));
        p = (*end == ',') ? end + 1 : end;
    }
    return out;
}

// LIBRARY MODE (BL-1310): `world_audit --seeds a,b,c [--pairs t:m,...]` builds
// each SHIPPED world (history on, the app's data layer) and audits the strait
// rule on it — natural counts, then each target pair. par_seeds.sh shards it.
static int run_library_mode(const std::vector<uint32_t>& seeds, const std::vector<std::pair<int, int>>& pairs)
{
    lua_state        lua;
    world_gen_config cfg{};
    works_registry   works;
    lua.load("scripts/world_gen.lua");
    cfg.load_from_lua(lua);
    lua.load("scripts/works.lua");
    works.load_from_lua(lua);
    recipe_registry reg; // the finish's settle runs on it (the size reading's majors)
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    reg.load_from_lua(lua);

    int checks = 0, fails = 0, cessions_total = 0, splits_seen = 0;
    std::printf("world_audit strait rule (BL-1310), %zu seed(s), %zu target pair(s) beside natural\n",
                seeds.size(), pairs.size());
    for (const uint32_t seed : seeds)
    {
        // (target total, target majors, launch): 0 the shipped close at 1960;
        // 1 an EMPIRES-ROUND launch (Exploration and Industrialisation off);
        // 2 an EXPLORATION-ROUND launch (Industrialisation off). Every launch
        // obeys the strait rule (Ben, 2026-10-17); the early ones by the
        // tail's post-pass, its cessions recorded in the last span's record.
        struct run_spec { int tt, mt, launch; };
        std::vector<run_spec> runs{ { 0, 0, 0 } };
        for (const auto& [t, m] : pairs) runs.push_back({ t, m, 0 });
        // 20:4, the wizard default, always: the nation size reading runs there.
        bool have_default = false;
        for (const run_spec& r : runs) have_default = have_default || (r.tt == 20 && r.mt == 4 && r.launch == 0);
        if (!have_default) runs.push_back({ 20, 4, 0 });
        runs.push_back({ 0, 0, 1 });
        runs.push_back({ 0, 0, 2 });
        for (const auto& [tt, mt, launch] : runs)
        {
            world_params p = arc_params(world_arc::shipped);
            p.seed                = seed;
            p.nation_total_target = tt;
            p.major_target        = mt;
            if (launch == 1) p.exploration_sim_enabled = false;
            if (launch >= 1) p.industrialisation_span_enabled = false;
            generation_report     rep{};
            era_minus_one_fixture fx{};
            world             sw = make_hard_coded_world(p, &rep, cfg, nullptr, &works, &fx);
            const strait_stats s  = audit_strait_rule(sw, rep);
            const int          ce = count_cessions(rep);
            const history_sim_state& hs = fx.industrialisation_state;
            cessions_total += ce;
            // CAPITALS, by realm, for comparing two builds (`CAP` lines):
            // the polity each nation folds from and its capital's raster tile.
            if (g_dump_capitals)
                for (const auto& be : rep.bodies)
                    if (be.is_homeworld)
                        for (std::size_t i = 0; i < be.nation_ids.size() && i < be.nation_polity.size(); ++i)
                        {
                            if (be.nation_polity[i] < 0) continue;
                            const auto nit = sw.nations.find(be.nation_ids[i]);
                            if (nit == sw.nations.end()) continue;
                            const auto tit = sw.tiles.find(nit->second.capital_tile);
                            const int  ct  = tit == sw.tiles.end() ? -1
                                           : tit->second.grid_y * sw.bodies.at(be.id).grid_width + tit->second.grid_x;
                            std::printf("CAP %d %d %u %d %d\n", launch, tt, seed, be.nation_polity[i], ct);
                        }
            char tag[32];
            if (launch == 1)      std::snprintf(tag, sizeof(tag), "empires");
            else if (launch == 2) std::snprintf(tag, sizeof(tag), "explorat");
            else std::snprintf(tag, sizeof(tag), tt > 0 ? "t%d/%d" : "natural", tt, mt);
            // The rule holds, AND the cessions are the history's own events
            // (one per closing-round step), AND the tail found nothing left.
            // § Territorial fragmentation, measured: "the cessions counted as
            // recorded events, so the sim still visibly produces the
            // fragmentation the rule resolves" -- at least one on every world.
            // An early launch ran no closing round: its cessions are the
            // tail's, recorded as events of the last span (at least one).
            const bool events_ok = launch == 0 ? (ce == hs.closing_cessions && ce > 0) : ce > 0;
            // No capital off its own land (Pass 5 seats inside the
            // territory), and the frozen cores intact: no opening cession moved
            // a tile of a realm's core group (the review's row -- a core that
            // drifted onto received ground ceded the mainland under it).
            // THE SPLIT, CHECKED (BL-1310 round 4 review): every ceded part
            // joined its receiver, and no province spanned two parts -- in the
            // closing rounds (the plan's counts) and in the tail (an early
            // launch's cessions, through the fixture). Both are 0 by
            // construction; these rows make that a check, not a claim.
            const int parts_detached  = hs.closing_parts_detached + fx.tail_parts_detached;
            const int provinces_split = hs.closing_provinces_split + fx.tail_provinces_split;
            splits_seen += hs.closing_cessions_split + fx.tail_cessions_split;
            const bool ok = s.illegal == 0 && events_ok && rep.handoff_violation.empty()
                         && s.capital_off == 0 && hs.closing_core_tiles_lost == 0
                         && parts_detached == 0 && provinces_split == 0;
            std::printf("        split: parts not joined %d, provinces split %d, split cessions %d (rounds) + %d (tail)\n",
                        parts_detached, provinces_split, hs.closing_cessions_split, fx.tail_cessions_split);
            std::printf("        under the floor, a larger piece kept as the core: %d (rounds) + %d (tail)\n",
                        hs.closing_cores_floor_moved, fx.tail_cores_floor_moved);
            std::printf("        core tiles lost %d | cores moved off a speck %d (rounds) + %d (tail)\n", hs.closing_core_tiles_lost,
                        hs.closing_cores_speck_moved, fx.tail_cores_speck_moved);
            ++checks;
            fails += ok ? 0 : 1;
            std::printf("%s", ok ? "PASS" : "FAIL");
            print_strait(tag, seed, s, ce);
            // NR-1080: no capital on a speck, on every launch (§ Pass 2e).
            {
                const capital_speck_stats cs = audit_capital_specks(sw, rep, std::set<entity_id>{});
                ++checks;
                fails += cs.all == 0 ? 0 : 1;
                std::printf("%s  SPECK %s seed %u: nations seated on a <%d-tile group with a larger province group elsewhere %d | seedless %d%s\n",
                            cs.all == 0 ? "PASS" : "FAIL", tag, seed, k_speck_tiles, cs.all, cs.seedless, cs.detail.c_str());
            }
            std::printf("        plan: cessions %d (islet %d, to unheld %d, late %d), split secessions %d, "
                        "reps off core %d, overrides %zu, nations %d%s%s\n",
                        hs.closing_cessions, hs.closing_cessions_islet, hs.closing_cessions_to_unheld,
                        hs.closing_cessions_late, hs.closing_secessions_split, hs.closing_reps_off_core,
                        hs.closing_cell_overrides.size(), hs.closing_predicted,
                        rep.handoff_violation.empty() ? "" : " | VIOLATION: ", rep.handoff_violation.c_str());
            std::printf("        split cessions %d, city states %d (judged at the close), islet tiles carried %d (rounds) + %d (tail)\n",
                        hs.closing_cessions_split, hs.closing_city_states, hs.closing_islet_tiles_carried,
                        fx.tail_islet_tiles_carried);
            // THE RULE MUST NOT STARVE THE MINORS (Ben, 2026-10-18; sec
            // Territorial fragmentation, measured). At the wizard default
            // (20:4), after the finish (majors are nations home to a
            // non-background corporation, nation_sizes.cpp's reading): the
            // minors' median tile count must reach 80% of the SAME seed's
            // pre-BL-1310 median (the table below), and the majors may hold at
            // most 80% of the land. A READING, never a clamp: nothing is tuned
            // to it.
            if (launch == 0 && tt == 20 && mt == 4)
            {
                recipe_registry seed_reg = reg;
                (void)finish_campaign_world(sw, rep, seed_reg, p, cfg, nullptr);
                entity_id home = null_entity;
                for (const auto& be : rep.bodies)
                    if (be.is_homeworld) { home = be.id; break; }
                std::set<entity_id> majors;
                for (const auto& kv : sw.nations) // BL-1407: majors are a saved fact
                    if (kv.second.is_major) majors.insert(kv.first);
                std::vector<int> minor_sizes;
                long total_tiles = 0, major_tiles = 0;
                int  under_floor = 0;
                for (const auto& kv : sw.nations)
                {
                    const nation_component& n = kv.second;
                    if (n.tiles.empty()) continue;
                    const auto t = sw.tiles.find(n.tiles.front());
                    if (t == sw.tiles.end() || t->second.body != home) continue;
                    const int sz = static_cast<int>(n.tiles.size());
                    total_tiles += sz;
                    if (majors.count(kv.first)) major_tiles += sz;
                    else minor_sizes.push_back(sz);
                    if (sz < 80) ++under_floor;
                }
                // NR-1077 / NR-1080 (Ben, 2026-10-25) — NO CAPITAL ON A SPECK,
                // read again after the finish (the majors are known here). A
                // capital on a group under 10 tiles while a larger province
                // group of its own lies elsewhere is the class a cession that
                // left a Pass 2b islet with its cedent produced (seed 1), and a
                // seat moved to a speck province (seed 38). Any nation fails.
                {
                    const capital_speck_stats cs = audit_capital_specks(sw, rep, majors);
                    ++checks;
                    const bool speck_ok = cs.all == 0;
                    fails += speck_ok ? 0 : 1;
                    std::printf("%s  SPECK seed %u 20:4 (finished): nations seated on a <%d-tile group with a larger province group elsewhere %d (majors %d of %zu) | seedless %d%s\n",
                                speck_ok ? "PASS" : "FAIL", seed, k_speck_tiles, cs.all, cs.majors, majors.size(), cs.seedless,
                                cs.detail.c_str());
                }
                std::sort(minor_sizes.begin(), minor_sizes.end(), [](int a, int b) { return a > b; });
                const int    median = minor_sizes.empty() ? 0 : minor_sizes[minor_sizes.size() / 2];
                const double share  = total_tiles ? 100.0 * static_cast<double>(major_tiles) / static_cast<double>(total_tiles) : 0.0;
                // THE BAR IS EACH SEED'S OWN (Ben, 2026-10-18): 80% of that
                // seed's PRE-BL-1310 minors' median. The baseline is
                // tools/verify/nation_sizes at 20:4 on aa3c6304 (the tree
                // before BL-1310), one row per library seed. A seed with no row
                // has no baseline and FAILS loudly -- it never passes.
                static const std::map<uint32_t, int> k_pre_bl1310_median = {
                    { 0, 390 },  { 9, 524 },  { 10, 547 }, { 11, 605 }, { 12, 455 }, { 13, 644 },
                    { 25, 608 }, { 28, 227 }, { 31, 566 }, { 32, 460 }, { 37, 456 }, { 38, 455 },
                    { 40, 471 }, { 41, 631 }, { 43, 236 }, { 46, 558 },
                };
                // ACCEPTED EXCEPTIONS (Ben, 2026-10-18): a seed Ben accepted
                // below its 80% bar, pinned at the reading he accepted, so a
                // further drop still fails. Seed 43: 145 against a bar of
                // 188.8 (pre-BL-1310 236) -- its many small realms seat on
                // their smaller piece, and the floor absorbs them (trace in
                // HANDOFF_SPRINT_56.md). The majors' 80% ceiling still applies.
                // Seeds 28 (181 against 181.6) and 13 (496 against 515.2),
                // accepted 2026-10-19: BL-1386's treasury rules move the
                // history's armies and conquests, and with them these two
                // worlds' minors.
                static const std::map<uint32_t, int> k_accepted_median = { { 13, 496 }, { 28, 181 }, { 43, 145 } };
                const auto   acc_it   = k_accepted_median.find(seed);
                const bool   accepted = acc_it != k_accepted_median.end();
                const auto   base_it  = k_pre_bl1310_median.find(seed);
                const bool   has_base = base_it != k_pre_bl1310_median.end();
                const double bar      = accepted ? static_cast<double>(acc_it->second)
                                      : has_base ? 0.8 * static_cast<double>(base_it->second) : 0.0;
                const bool   size_ok  = has_base && static_cast<double>(median) >= bar && share <= 80.0;
                ++checks;
                fails += size_ok ? 0 : 1;
                if (!has_base)
                    std::printf("FAIL  SIZE seed %u 20:4: NO PRE-BL-1310 BASELINE for this seed (median %d, majors hold %.1f%%)\n",
                                seed, median, share);
                else
                    std::printf("%s  SIZE seed %u 20:4: minors %zu median %d (bar %.1f = %s, pre-BL-1310 %d) | majors hold %.1f%% (<= 80) | under floor(80) %d\n",
                                size_ok ? (accepted ? "ACCEPTED" : "PASS") : "FAIL", seed, minor_sizes.size(), median, bar,
                                accepted ? "the reading Ben accepted" : "80% of", base_it->second, share, under_floor);
            }
            std::fflush(stdout);
        }
    }
    // The split happens at all: at least one exclave split among two or more
    // neighbours over the worlds this run built (each shard: one library seed).
    ++checks;
    fails += splits_seen > 0 ? 0 : 1;
    std::printf("%s  splits seen: %d exclave(s) split among 2+ neighbours\n", splits_seen > 0 ? "PASS" : "FAIL", splits_seen);
    std::printf("\nSUMMARY strait rule: %d check(s), %d failure(s), %d cession event(s) -- %s\n", checks, fails,
                cessions_total, fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}

int main(int argc, char** argv)
{
    {
        std::vector<uint32_t>            seeds;
        std::vector<std::pair<int, int>> pairs;
        for (int i = 1; i < argc; ++i)
        {
            if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc) seeds = parse_seed_list(argv[++i]);
            else if (std::strcmp(argv[i], "--dump-capitals") == 0) g_dump_capitals = true;
            else if (std::strcmp(argv[i], "--pairs") == 0 && i + 1 < argc)
            {
                const std::vector<uint32_t> v = parse_seed_list(argv[++i]); // "t:m" read below
                (void)v;
                const char* s = argv[i];
                while (*s)
                {
                    char* e1 = nullptr;
                    const long t = std::strtol(s, &e1, 10);
                    if (e1 == s || *e1 != ':') break;
                    char* e2 = nullptr;
                    const long m = std::strtol(e1 + 1, &e2, 10);
                    pairs.emplace_back(static_cast<int>(t), static_cast<int>(m));
                    s = (*e2 == ',') ? e2 + 1 : e2;
                    if (e2 == e1 + 1) break;
                }
            }
        }
        if (!seeds.empty()) return run_library_mode(seeds, pairs);
    }

    generation_report report;
    world w = make_hard_coded_world(no_prehistory(), &report);

    // --- BL-331: starting military presence (built under the BL-330 label,
    // 2eb8654 — a mislabeled commit; this is the first PASS/FAIL check the
    // seeding has ever had). ---
    {
        bool has_base = false;
        const auto pcit = w.nations.find(w.player_entity); // BL-1407: the seat is a nation
        if (pcit != w.nations.end())
        {
            for (entity_id asset : pcit->second.state_assets)
            {
                const auto bit = w.buildings.find(asset);
                if (bit != w.buildings.end() && bit->second.type == building_type::military_base)
                {
                    has_base = true;
                    break;
                }
            }
        }
        bool has_unit = false;
        for (const auto& [uid, uc] : w.units)
        {
            if (uc.owner == w.player_entity && uc.count > 0)
            {
                has_unit = true;
                break;
            }
        }
        std::printf("\nBL-331 starting military presence: base=%s unit=%s : %s\n",
                    has_base ? "yes" : "no", has_unit ? "yes" : "no",
                    (has_base && has_unit) ? "PASS" : "FAIL");
    }

    // --- S2: Kepler terrain-axis histogram (BL-519: substrate x cover) ---
    const entity_id kepler = w.home_body;
    std::map<int, int> hist;
    int land = 0, total = 0;
    int forest = 0, wetland = 0;
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != kepler)
            continue;
        ++total;
        ++hist[static_cast<int>(tc.substrate) * 16 + static_cast<int>(tc.cover)];
        if (!is_water(tc.substrate))
            ++land;
        if (tc.cover == terrain_cover::forest)  ++forest;
        if (tc.cover == terrain_cover::marsh) ++wetland;
    }

    std::printf("Kepler tiles: %d total, %d land\n", total, land);
    // Denominator is LAND, not all tiles (BL-338): both biomes are land-only.
    const float fw_frac = land ? 100.0f * static_cast<float>(forest + wetland) / land : 0.0f;
    std::printf("  forest=%d (%.2f%% of land)  wetland=%d (%.2f%% of land)  forest+wetland=%.2f%% of land\n",
                forest, land ? 100.0f * forest / land : 0.0f,
                wetland, land ? 100.0f * wetland / land : 0.0f, fw_frac);
    std::printf("  S2 target forest+wetland >= 3%% of land: %s\n",
                fw_frac >= 3.0f ? "PASS" : "FAIL");

    // --- S3 (BL-231): per-body landform histogram ---
    // The renderer keys only on the substrate/cover pair, so the landform axis has never been
    // looked at. It drives build cost (x1.0-x2.0), hazard, habitability and
    // mineral richness, so how SPARSE each landform is decides how it should be
    // drawn: a landform holding a third of a body wants a field treatment, one
    // holding a few percent wants a marker. Reported per body because the
    // profile differs sharply (airless bodies are cratered; atmospheric ones
    // are not). Measurement first — see backlog.json BL-231 § Step 1.
    static const char* kLandformName[7] = {
        "plains", "highland", "mountain", "canyon", "valley", "crater", "rift" };
    constexpr int kNumLandform = 7;

    std::printf("\nLandform distribution (BL-231; land tiles only, ocean excluded)\n");
    std::map<entity_id, std::array<int, kNumLandform>> per_body;
    std::map<entity_id, int> per_body_land;
    std::array<int, kNumLandform> system_hist{};
    int system_land = 0;
    for (const auto& [tid, tc] : w.tiles)
    {
        if (is_water(tc.substrate))
            continue;
        const int lf = static_cast<int>(tc.landform);
        if (lf < 0 || lf >= kNumLandform)
            continue;
        ++per_body[tc.body][lf];
        ++per_body_land[tc.body];
        ++system_hist[lf];
        ++system_land;
    }

    for (const auto& [bid, hist_lf] : per_body)
    {
        const auto bit = w.bodies.find(bid);
        const int  n   = per_body_land[bid];
        std::printf("  %-12s (%4d land)", bit == w.bodies.end() ? "?" : bit->second.name.c_str(), n);
        for (int i = 0; i < kNumLandform; ++i)
            std::printf("  %s %.1f%%", kLandformName[i], n ? 100.0f * hist_lf[i] / n : 0.0f);
        std::printf("\n");
    }

    std::printf("  SYSTEM       (%4d land)", system_land);
    for (int i = 0; i < kNumLandform; ++i)
        std::printf("  %s %.1f%%", kLandformName[i],
                    system_land ? 100.0f * system_hist[i] / system_land : 0.0f);
    std::printf("\n");

    // The generation pipeline can emit all seven; a landform absent system-wide
    // means a pass stopped firing (a silently flattened mix), which is exactly
    // the regression this check exists to catch.
    int landform_absent = 0;
    for (int i = 0; i < kNumLandform; ++i)
        if (system_hist[i] == 0)
        {
            ++landform_absent;
            std::printf("  BAD: landform '%s' appears on no body\n", kLandformName[i]);
        }
    std::printf("  BL-231 R1 every landform generation can produce appears somewhere: %s\n",
                landform_absent == 0 ? "PASS" : "FAIL");

    // --- S5 (BL-233): terrain combat value, as the ladder now prices it ---
    // ADOPTED 2026-07-31 — history_ladder.cpp no longer has an is_barrier bool. Its
    // barrier_q is now two terms:
    //     barrier_q = mean(terrain_resistance) over LAND
    //               + ocean_share * k_ocean_barrier_weight / 1000
    // with the weight at 750 (Ben's call; chosen by sweep to hold the political map
    // while the land term fixes the grading defect).
    //
    // This section keeps reporting the retired binary alongside it. That is deliberate
    // and NOT dead weight: the binary is the only fixed reference point for how far the
    // field has moved, and the per-body defence/attrition means are the tuning guard the
    // item asked for — a change that flattens the graded field shows up here as means
    // collapsing toward each other, which no assertion elsewhere would catch.
    //
    // Denominators differ between the two by design, and the labels say so: the binary
    // was a share of ALL tiles (ocean included), the graded land term is a mean over
    // LAND only, because water is now priced by its own term rather than smuggled into
    // the terrain signal.
    //
    // Downstream, exactly as the ladder computes it:
    //   fragmentation_q = (barrier_q*6 + cradle_q*4)/10   -> d_frag  = 0.6 * d_barrier
    //   polity_seeds    = cradles + fragmentation_q*12/1000 -> d_seeds = 12 * d_frag / 1000
    constexpr int k_ocean_barrier_weight = 750; // Mirrors history_ladder.cpp.
    std::printf("\nTerrain combat value (BL-233; retired binary vs the ADOPTED graded + ocean field)\n");
    for (const auto& [bid, bc] : w.bodies)
    {
        if (bc.grid_width <= 0 || bc.grid_height <= 0)
            continue;
        const int total_n = bc.grid_width * bc.grid_height;

        int bin_barrier = 0, bin_ocean = 0, graded_sum = 0, land_n = 0;
        long long def_sum = 0, att_sum = 0;
        for (const auto& [tid, tc] : w.tiles)
        {
            if (tc.body != bid)
                continue;
            const bool ocean = (is_water(tc.substrate));

            // Replicate the ladder's is_barrier exactly (history_ladder.cpp § is_barrier).
            const bool bin = ocean
                          || tc.landform == terrain_landform::mountain
                          || tc.landform == terrain_landform::canyon
                          || tc.substrate == terrain_substrate::barren
                          || tc.substrate == terrain_substrate::icy;
            if (bin)   ++bin_barrier;
            if (ocean) ++bin_ocean;

            if (!ocean)
            {
                ++land_n;
                graded_sum += terrain_resistance(tc.substrate, tc.cover, tc.cover_density,
                                                 tc.landform);
                def_sum    += terrain_defence(tc.substrate, tc.cover, tc.cover_density,
                                              tc.landform);
                att_sum    += terrain_attrition(tc.substrate, tc.cover, tc.cover_density,
                                                tc.landform);
            }
        }
        if (total_n <= 0)
            continue;

        const int bin_q = std::clamp((bin_barrier * 1000) / total_n, 0, 1000);

        // The ADOPTED field, computed exactly as history_ladder.cpp Pass 1/3 does:
        // graded resistance averaged over LAND, plus the ocean share at its own weight.
        const int ocean_q     = std::clamp((bin_ocean * 1000) / total_n, 0, 1000);
        const int land_res_q  = land_n ? std::clamp(graded_sum / land_n, 0, 1000) : 0;
        const int adopted_q   = std::clamp(
            land_res_q + (ocean_q * k_ocean_barrier_weight) / 1000, 0, 1000);

        const int  d_barrier = adopted_q - bin_q;
        const float d_frag   = 0.6f * static_cast<float>(d_barrier);
        const float d_seeds  = 12.0f * d_frag / 1000.0f;

        std::printf("  %-12s barrier_q was %4d (binary, ocean %4d)  now %4d (land %4d + ocean %4d x%.2f)  delta %+5d\n",
                    bc.name.c_str(), bin_q, ocean_q, adopted_q, land_res_q, ocean_q,
                    k_ocean_barrier_weight / 1000.0, d_barrier);
        std::printf("                -> fragmentation_q %+.1f, polity_seeds %+.2f | land means: defence %d, attrition %d\n",
                    static_cast<double>(d_frag), static_cast<double>(d_seeds),
                    land_n ? static_cast<int>(def_sum / land_n) : 0,
                    land_n ? static_cast<int>(att_sum / land_n) : 0);
    }

    // --- S4 (BL-232): landform CONTIGUITY ---
    // BL-231 draws one glyph per tile, so a run of three mountains reads as three
    // identical icons rather than as one range. Bridging them into a spanning ridge
    // only pays if runs actually exist: Pass 5 grows mountain from seeds where ring 0
    // is mountain but ring 1 is only 30% mountain, so clusters may be scattered specks.
    // Reported as the share of tiles with 0 / 1 / 2+ same-landform CARDINAL neighbours
    // (the 4 directions the span idiom uses, matching BL-172's road edges) plus the
    // share that are fully interior — those three numbers decide whether the spanning
    // case or the filled-interior case is the common one, or neither is. Measurement
    // before the visual, exactly as S3 gated BL-231.
    const terrain_landform kLinear[3] = { terrain_landform::mountain,
                                          terrain_landform::rift,
                                          terrain_landform::canyon };

    // Per-body raster of landform, so a cardinal neighbour is an O(1) lookup and the
    // column wrap matches the canvas (columns wrap, rows do not).
    std::map<entity_id, std::vector<int>> body_lf; // -1 = ocean or absent
    for (const auto& [bid, bc] : w.bodies)
        if (bc.grid_width > 0 && bc.grid_height > 0)
            body_lf[bid].assign(static_cast<std::size_t>(bc.grid_width) * bc.grid_height, -1);
    for (const auto& [tid, tc] : w.tiles)
    {
        const auto bit = body_lf.find(tc.body);
        if (bit == body_lf.end() || is_water(tc.substrate))
            continue;
        const auto bc = w.bodies.find(tc.body);
        if (bc == w.bodies.end())
            continue;
        const std::size_t idx = static_cast<std::size_t>(tc.grid_y) * bc->second.grid_width + tc.grid_x;
        if (idx < bit->second.size())
            bit->second[idx] = static_cast<int>(tc.landform);
    }

    std::printf("\nLandform contiguity (BL-232; same-landform cardinal neighbours)\n");
    for (terrain_landform lf : kLinear)
    {
        const int lfi = static_cast<int>(lf);
        int n0 = 0, n1 = 0, n2plus = 0, interior = 0, tiles = 0;
        entity_id ex_run_body = null_entity, ex_lone_body = null_entity;
        int ex_run_col = 0, ex_run_row = 0, ex_lone_col = 0, ex_lone_row = 0;
        for (const auto& [bid, raster] : body_lf)
        {
            const auto bc = w.bodies.find(bid);
            if (bc == w.bodies.end())
                continue;
            const int bw = bc->second.grid_width, bh = bc->second.grid_height;
            for (int row = 0; row < bh; ++row)
                for (int col = 0; col < bw; ++col)
                {
                    if (raster[static_cast<std::size_t>(row) * bw + col] != lfi)
                        continue;
                    ++tiles;
                    int same = 0;
                    static const int off[4][2] = {{+1, 0}, {-1, 0}, {0, +1}, {0, -1}};
                    for (auto& d : off)
                    {
                        const int nrow = row + d[1];
                        if (nrow < 0 || nrow >= bh)
                            continue;
                        const int ncol = ((col + d[0]) % bw + bw) % bw; // columns wrap
                        if (raster[static_cast<std::size_t>(nrow) * bw + ncol] == lfi)
                            ++same;
                    }
                    if (same == 0)      ++n0;
                    else if (same == 1) ++n1;
                    else                ++n2plus;
                    if (same == 4)      ++interior;

                    // Prefer the home body for the exemplars — that is the surface a
                    // verify capture opens on without a survey reveal.
                    const bool home = (bid == w.home_body);
                    if (same >= 1 && (ex_run_body == null_entity
                                      || (home && ex_run_body != w.home_body)))
                    { ex_run_body = bid; ex_run_col = col; ex_run_row = row; }
                    if (same == 0 && (ex_lone_body == null_entity
                                      || (home && ex_lone_body != w.home_body)))
                    { ex_lone_body = bid; ex_lone_col = col; ex_lone_row = row; }
                }
        }
        const float pc = tiles ? 100.0f / tiles : 0.0f;
        std::printf("  %-9s %5d tiles | isolated %.1f%%  end-of-run %.1f%%  in-run(2+) %.1f%%  interior %.1f%%\n",
                    kLandformName[lfi], tiles, n0 * pc, n1 * pc, n2plus * pc, interior * pc);
        // Exemplar coordinates for scripts/verify/landform_relief.lua, which needs a
        // real spanning run and a real lone tile to point the cursor at. Printed rather
        // than eyeballed off a screenshot so a generation change that moves them shows
        // up here instead of silently turning a capture into a picture of empty ground.
        if (ex_run_body != null_entity)
        {
            const auto eb = w.bodies.find(ex_run_body);
            std::printf("      exemplar run  : %s [%d,%d]\n",
                        eb == w.bodies.end() ? "?" : eb->second.name.c_str(),
                        ex_run_col, ex_run_row);
        }
        if (ex_lone_body != null_entity)
        {
            const auto eb = w.bodies.find(ex_lone_body);
            std::printf("      exemplar lone : %s [%d,%d]\n",
                        eb == w.bodies.end() ? "?" : eb->second.name.c_str(),
                        ex_lone_col, ex_lone_row);
        }
    }

    // --- S1: extraction asset placement audit ---
    const resource_type extractable[] = {
        resource_type::iron_ore, resource_type::petroleum,
        resource_type::water, resource_type::agricultural_produce };

    int extraction_assets = 0, bad = 0;
    for (const auto& [cid, corp] : w.gen_charters)
    {
        for (entity_id bid : corp.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end() || bit->second.type != building_type::extraction_site)
                continue;
            ++extraction_assets;
            const auto tit = w.tiles.find(bit->second.tile);
            if (tit == w.tiles.end())
            {
                ++bad; std::printf("  BAD: extraction asset on missing tile\n"); continue;
            }
            const tile_component& tc = tit->second;
            bool ocean = (is_water(tc.substrate));
            float dep = 0.0f;
            for (resource_type r : extractable) dep += tc.resource_deposit[ri(r)];
            const float tgt = tc.resource_deposit[ri(bit->second.target_resource)];
            if (ocean || dep <= 0.0f || tgt <= 0.0f)
            {
                ++bad;
                std::printf("  BAD: extraction asset corp=%u ocean=%d extractable_dep=%.3f target_dep=%.3f\n",
                            static_cast<unsigned>(cid), ocean ? 1 : 0, dep, tgt);
            }
        }
    }
    std::printf("Extraction assets: %d, invalid placements: %d\n", extraction_assets, bad);
    std::printf("  S1 R2 all extraction assets on a valid extractable deposit: %s\n",
                bad == 0 ? "PASS" : "FAIL");

    // --- placement-rules seam (v0.0.5 R3/R4): every placed extraction asset
    // passes the reusable can_place() check; an ocean tile and a zero-deposit
    // tile are rejected. ---
    int seam_bad = 0;
    for (const auto& [cid, corp] : w.gen_charters)
    {
        for (entity_id bid : corp.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end() || bit->second.type != building_type::extraction_site)
                continue;
            const auto tit = w.tiles.find(bit->second.tile);
            if (tit == w.tiles.end()
                || !placement_rules::can_place(tit->second,
                                               building_type::extraction_site,
                                               bit->second.target_resource))
            {
                ++seam_bad;
                std::printf("  BAD: placed extraction asset corp=%u fails can_place\n",
                            static_cast<unsigned>(cid));
            }
        }
    }
    // Negative controls: ocean and zero-deposit tiles must be rejected.
    tile_component ocean_tile{};
    ocean_tile.substrate = terrain_substrate::ocean;
    ocean_tile.resource_deposit[ri(resource_type::iron_ore)] = 10.0f; // deposit present, but ocean
    const bool ocean_rejected = !placement_rules::can_place(
        ocean_tile, building_type::extraction_site, resource_type::iron_ore);

    tile_component dry_tile{};
    dry_tile.substrate = terrain_substrate::barren; // land, but no iron deposit
    const bool zero_rejected = !placement_rules::can_place(
        dry_tile, building_type::extraction_site, resource_type::iron_ore);
    // A processing facility on the same dry land tile is valid (target ignored).
    const bool proc_ok = placement_rules::can_place(
        dry_tile, building_type::processing_facility, resource_type::iron_ore);

    if (!ocean_rejected) { ++seam_bad; std::printf("  BAD: can_place accepted an ocean tile\n"); }
    if (!zero_rejected)  { ++seam_bad; std::printf("  BAD: can_place accepted a zero-deposit extraction tile\n"); }
    if (!proc_ok)        { ++seam_bad; std::printf("  BAD: can_place rejected a valid processing tile\n"); }
    std::printf("  v0.0.5 R3/R4 placement_rules::can_place agrees with placement + negative controls: %s\n",
                seam_bad == 0 ? "PASS" : "FAIL");

    // --- Deposit depletion (Brief B, R1): resource_remaining seeded from richness ---
    // Generation seeds resource_remaining[r] = resource_deposit[r] * deposit_reserve_factor
    // (400.0) for every non-zero deposit; absent deposits stay at zero.
    constexpr float reserve_factor = 400.0f;
    int deposits_checked = 0, seed_bad = 0;
    for (const auto& [tid, tc] : w.tiles)
    {
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float dep = tc.resource_deposit[r];
            const float rem = tc.resource_remaining[r];
            if (dep > 0.0f)
            {
                ++deposits_checked;
                const float want = dep * reserve_factor;
                if (rem < want - 1e-2f || rem > want + 1e-2f)
                {
                    if (seed_bad < 5)
                        std::printf("  BAD: tile %u res %zu remaining=%.3f want=%.3f\n",
                                    static_cast<unsigned>(tid), r, rem, want);
                    ++seed_bad;
                }
            }
            else if (rem != 0.0f)
            {
                ++seed_bad; // a reserve with no deposit is wrong too
            }
        }
    }
    std::printf("Deposit reserves: %d non-zero deposits, %d mis-seeded\n", deposits_checked, seed_bad);
    std::printf("  B R1 resource_remaining = richness * %.0f for every deposit: %s\n",
                reserve_factor, seed_bad == 0 ? "PASS" : "FAIL");

    // --- C2 (orphan-island assignment, R1): every non-ocean Kepler land tile is
    // owned by a nation after the orphan-island post-pass. ---
    int kepler_land = 0, unclaimed_land = 0;
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != kepler || is_water(tc.substrate))
            continue;
        ++kepler_land;
        if (w.tile_to_nation.find(tid) == w.tile_to_nation.end())
            ++unclaimed_land;
    }
    std::printf("Kepler land ownership: %d land tiles, %d unclaimed\n", kepler_land, unclaimed_land);
    std::printf("  C2 R1 every non-ocean land tile assigned to a nation: %s\n",
                unclaimed_land == 0 ? "PASS" : "FAIL");

    // --- B4 (corp starting-holdings, R1/R3): each corp opens with a lean,
    // focus-shaped holding count — no corp exceeds its focus ceiling, and every
    // corp is a going concern (>= 1 asset). Counts below the focus minimum are
    // legitimate on a cramped, deposit-poor nation, so they are reported but do
    // not fail. Focus ceilings mirror holdings_range in corporation_generation.cpp:
    // extraction 3..4, processing 2..3, trade 2..3 (trade was 1..2 until
    // 2026-08-16: a 1-draw opened a corp holding nothing but a port, which has
    // base_rate 0 and so no income at all — see holdings_range's own note). ---
    //
    // THIRD hand-mirrored table to drift out of date in one session, after
    // ai_skill_harness's richness fields and both harnesses' base_rate. Each time
    // the harness reported confidently against numbers the game no longer used.
    auto focus_bounds = [](industrial_focus f) -> std::pair<int,int> {
        switch (f)
        {
            case industrial_focus::extraction: return { 3, 4 };
            case industrial_focus::processing: return { 2, 3 };
            case industrial_focus::trade:      return { 2, 3 };
        }
        return { 2, 3 };
    };
    auto focus_name = [](industrial_focus f) -> const char* {
        switch (f)
        {
            case industrial_focus::extraction: return "extraction";
            case industrial_focus::processing: return "processing";
            case industrial_focus::trade:      return "trade";
        }
        return "?";
    };
    int holdings_bad = 0;
    for (const auto& [cid, corp] : w.gen_charters)
    {
        // BL-476 seeds one `military_base` into EVERY corp's assets (it was the
        // player's alone under BL-330). `holdings_range` governs the corp's
        // ECONOMIC holdings and never counted that base, so counting raw
        // assets.size() here compares against a ceiling one too low for every
        // corp — it only reds when a draw happens to land at the top of its
        // range, which is why it survived until a world-density change made one
        // do so. Count economic holdings only; the seeded base is audited by
        // rival_military_seeding_harness, which owns that promise.
        //
        // (This is the FOURTH drift of a hand-mirrored table noted in this file.
        //  The count is now derived from the world rather than mirrored.)
        int count = 0;
        for (const entity_id aid : corp.assets)
        {
            const auto bit = w.buildings.find(aid);
            if (bit != w.buildings.end() && bit->second.type == building_type::military_base)
                continue;
            ++count;
        }
        const auto [lo, hi] = focus_bounds(corp.focus);
        const bool over  = count > hi;
        const bool empty = count < 1;
        if (over || empty)
        {
            ++holdings_bad;
            std::printf("  BAD: corp=%u focus=%s holdings=%d outside [1,%d]\n",
                        static_cast<unsigned>(cid), focus_name(corp.focus), count, hi);
        }
        else
        {
            std::printf("  corp=%u focus=%s holdings=%d (range %d..%d%s)\n",
                        static_cast<unsigned>(cid), focus_name(corp.focus), count, lo, hi,
                        count < lo ? ", below min — cramped nation" : "");
        }
    }
    std::printf("  B4 R1 every corp holding count within its focus ceiling (>=1): %s\n",
                holdings_bad == 0 ? "PASS" : "FAIL");

    // --- BL-116 (generated corp starting stockpile): the opening stockpile is
    // generated from industrial focus + starting capital (replaces BL-115's
    // fixed give). make_hard_coded_world(no_prehistory()) is the cold generation state (no
    // pre-game ticks), so pools equal the generated give exactly. Checks:
    //   R1 every corp with holdings opens with a non-empty stockpile on its home
    //      body, scoped to the seven prototype resources (nothing else stocked);
    //   R2 focus correlation — extraction corps open richer in raws than trade;
    //   R3 determinism — a second generation yields identical stockpiles. ---
    auto home_body_of = [&](const charter_holding& corp) -> entity_id {
        for (entity_id bid : corp.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end()) continue;
            const auto tit = w.tiles.find(bit->second.tile);
            if (tit != w.tiles.end()) return tit->second.body;
        }
        return null_entity;
    };
    const resource_type prototype_set[] = {
        resource_type::iron_ore, resource_type::petroleum, resource_type::water,
        resource_type::agricultural_produce, resource_type::steel,
        resource_type::refined_fuel, resource_type::food_rations };
    const resource_type raw_set[] = {
        resource_type::iron_ore, resource_type::petroleum,
        resource_type::water, resource_type::agricultural_produce };
    auto is_prototype = [&](std::size_t r) {
        for (resource_type p : prototype_set) if (ri(p) == r) return true;
        return false;
    };

    int stock_corps = 0, stock_bad = 0;
    double ext_raw_sum = 0.0, trade_raw_sum = 0.0;
    int ext_n = 0, trade_n = 0;
    for (const auto& [cid, corp] : w.gen_charters)
    {
        if (corp.assets.empty()) continue;         // no holdings → no home body
        const entity_id hb = home_body_of(corp);
        if (hb == null_entity)
        {
            ++stock_bad;
            std::printf("  BAD: corp=%u has assets but no resolvable home body\n",
                        static_cast<unsigned>(cid));
            continue;
        }
        ++stock_corps;
        // BL-1265 (CORPORATION_GENERATION.md § Pass 4b): the opening stock is
        // on the SHELVES of the markets the corp sits in, not in a pool of its
        // own — so R1 reads those shelves: non-empty and prototype-scoped. The
        // per-corp focus split (R2) cannot be read off a shared shelf.
        float total = 0.0f;
        // BL-1407: a charter is no owner (corp_markets reads an owner's list), so
        // the markets it sits in are its own buildings' tile markets.
        std::set<entity_id> charter_markets;
        for (const entity_id bid : corp.assets)
            if (const auto bit = w.buildings.find(bid); bit != w.buildings.end())
                if (const entity_id m = market_for_tile(w, bit->second.tile); m != null_entity)
                    charter_markets.insert(m);
        for (const entity_id mid : charter_markets)
        {
            const auto& q = w.markets.at(mid).inventory;
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                total += q[r];
                if (!is_prototype(r) && q[r] != 0.0f)
                {
                    ++stock_bad;
                    std::printf("  BAD: corp=%u market=%u non-prototype res %zu stocked (%.2f)\n",
                                static_cast<unsigned>(cid), static_cast<unsigned>(mid), r, q[r]);
                }
            }
        }
        if (total <= 0.0f)
        {
            ++stock_bad;
            std::printf("  BAD: corp=%u sits on no stocked shelf\n",
                        static_cast<unsigned>(cid));
        }
        (void)raw_set;
        (void)corp;
    }
    std::printf("Corp starting stockpiles: %d stocked corps, %d discrepancies\n",
                stock_corps, stock_bad);
    std::printf("  BL-116 R1 every corp opens non-empty, prototype-scoped, on its home body: %s\n",
                stock_bad == 0 ? "PASS" : "FAIL");

    bool focus_ok = true;
    if (ext_n > 0 && trade_n > 0)
    {
        const double ext_mean   = ext_raw_sum / ext_n;
        const double trade_mean = trade_raw_sum / trade_n;
        focus_ok = ext_mean > trade_mean;
        std::printf("  raw-stock mean: extraction=%.1f (n=%d)  trade=%.1f (n=%d)\n",
                    ext_mean, ext_n, trade_mean, trade_n);
        std::printf("  BL-116 R2 extraction opens richer in raws than trade: %s\n",
                    focus_ok ? "PASS" : "FAIL");
    }
    else
    {
        std::printf("  BL-116 R2 focus correlation: SKIP (need >=1 extraction and >=1 trade; ext=%d trade=%d)\n",
                    ext_n, trade_n);
    }

    world w2 = make_hard_coded_world(no_prehistory());
    bool det_ok = (w.markets.size() == w2.markets.size());
    int det_bad = 0;
    for (const auto& [mid, mc] : w.markets)
    {
        const auto it2 = w2.markets.find(mid);
        if (it2 == w2.markets.end() || it2->second.inventory != mc.inventory) ++det_bad;
    }
    if (det_bad != 0) det_ok = false;
    std::printf("  BL-116 R3 opening shelves identical across two generations (%zu markets, %d mismatched): %s\n",
                w.markets.size(), det_bad, det_ok ? "PASS" : "FAIL");

    const bool stockpile_ok = (stock_bad == 0) && focus_ok && det_ok;

    // --- BL-040: full raw-set deposit-distribution audit ---
    // Every raw resource the full-set pass adds must now be authored somewhere in
    // the world, and the seeded rarity ordering must hold: the ultra-rare
    // platinum-group metals must sit on strictly fewer tiles than common copper.
    struct raw_res { resource_type res; const char* name; };
    const raw_res added[] = {
        { resource_type::coal,                  "coal" },
        { resource_type::silica,                "silica" },
        { resource_type::copper_ore,            "copper_ore" },
        { resource_type::rare_earth_ore,        "rare_earth_ore" },
        { resource_type::iron_nickel_ore,       "iron_nickel_ore" },
        { resource_type::platinum_group_metals, "platinum_group_metals" },
    };
    std::map<int, int> tile_count; // resource index -> tiles bearing it
    for (const auto& [tid, tc] : w.tiles)
        for (const raw_res& a : added)
            if (tc.resource_deposit[ri(a.res)] > 0.0f)
                ++tile_count[static_cast<int>(a.res)];

    int absent = 0;
    for (const raw_res& a : added)
    {
        const int n = tile_count[static_cast<int>(a.res)];
        std::printf("  %-22s deposits on %d tiles\n", a.name, n);
        if (n == 0) { ++absent; std::printf("  BAD: %s authored on no tile\n", a.name); }
    }
    const int pgm    = tile_count[static_cast<int>(resource_type::platinum_group_metals)];
    const int copper = tile_count[static_cast<int>(resource_type::copper_ore)];
    const bool ordering_ok = pgm < copper;
    std::printf("  BL-040 R1 full raw set authored (all 6 additions present): %s\n",
                absent == 0 ? "PASS" : "FAIL");
    std::printf("  BL-040 R2 rarity ordering (PGM %d < copper %d): %s\n",
                pgm, copper, ordering_ok ? "PASS" : "FAIL");

    // --- BL-053: size-floor + size-variance audit ---
    // Only Kepler generates nations, so w.nations is the Kepler political layer.
    // The nation COUNT is emergent (seeds scale with land area; every nation below
    // the minimum viable territory is absorbed), so the load-bearing assertion is
    // the floor itself, not a target count. The count band is a wide sanity guard.
    //
    // REPOINTED 2026-07-30 (BL-221, Ben's call). The history ladder now DERIVES
    // the merge floor and the seed density from generated fragmentation, so
    // `nation_params{}.min_nation_tiles` is no longer the floor the generator
    // actually used — it is only the base the ladder modulates. Asserting the
    // old literal would test a constant that no longer exists.
    //
    // What IS still guaranteed by construction is the clamp in
    // nation_params_from_ladder: the derived floor can never fall below half the
    // base. So that is what R1 asserts — the generator's real invariant, not a
    // band widened to accommodate a failure. Kepler moved 14 -> 43 nations, which
    // is roughly the ~45 docs/lore/HISTORY.md asserts; Ben's direction was to let
    // culturally distinct polities emerge here and let a later war/consolidation
    // stage narrow the count, rather than tuning the ladder to fit this file.
    const int floor_base  = nation_params{}.min_nation_tiles;
    const int floor_tiles = floor_base / 2;
    const int nation_n    = static_cast<int>(w.nations.size());
    int min_tiles = -1, max_tiles = 0;
    for (const auto& [nid, nat] : w.nations)
    {
        const int t = static_cast<int>(nat.tiles.size());
        if (min_tiles < 0 || t < min_tiles) min_tiles = t;
        if (t > max_tiles) max_tiles = t;
    }
    if (min_tiles < 0) min_tiles = 0;
    const bool floor_ok    = nation_n > 0 && min_tiles >= floor_tiles;
    // Ceiling raised 40 -> 90 for the same reason. This is a RUNAWAY guard, not
    // a target: it still catches a ladder bug that seeds hundreds of polities,
    // while leaving room for the emergent count the premise wants.
    const bool count_ok    = nation_n >= 6 && nation_n <= 90;
    const bool variance_ok = min_tiles > 0 && max_tiles >= 3 * min_tiles;
    std::printf("Nations: %d (min tiles %d, max tiles %d; floor %d)\n",
                nation_n, min_tiles, max_tiles, floor_tiles);
    std::printf("  BL-053 R1 every nation clears the ladder's guaranteed floor (>= %d tiles, half of base %d): %s\n",
                floor_tiles, floor_base, floor_ok ? "PASS" : "FAIL");
    std::printf("  BL-053 R2 strong size variance (max >= 3x min): %s\n",
                variance_ok ? "PASS" : "FAIL");
    std::printf("  BL-053 R3 emergent nation count inside the runaway guard ([6,90]): %s\n",
                count_ok ? "PASS" : "FAIL");

    // --- BL-096: resource-carved market generation ---
    // Markets are population-anchored but resource-carved: a nation's territory is
    // split into more or fewer markets by its tradeable-resource concentration, with
    // nations as the carving actor. Assert the carve produced a plural, cross-nation
    // market map and that it is deterministic (same seed -> same map).
    int kepler_markets = 0;
    std::vector<entity_id> centre_tiles;
    std::set<entity_id>    market_nations;
    for (const auto& [mid, mc] : w.markets)
    {
        if (mc.body != kepler)
            continue;
        ++kepler_markets;
        centre_tiles.push_back(mc.centre_tile);
        if (mc.centre_tile != null_entity)
        {
            const auto nit = w.tile_to_nation.find(mc.centre_tile);
            if (nit != w.tile_to_nation.end())
                market_nations.insert(nit->second);
        }
    }
    std::sort(centre_tiles.begin(), centre_tiles.end());
    const bool market_count_ok = kepler_markets >= 2 && kepler_markets <= 20;
    const bool cross_nation_ok = market_nations.size() >= 2;
    std::printf("Kepler markets: %d (anchored across %zu nations)\n",
                kepler_markets, market_nations.size());
    std::printf("  BL-096 R1 resource-carved market count in [2,20]: %s\n",
                market_count_ok ? "PASS" : "FAIL");
    std::printf("  BL-096 R2 markets span >= 2 nations (nation-carved): %s\n",
                cross_nation_ok ? "PASS" : "FAIL");

    world wm2 = make_hard_coded_world(no_prehistory());
    std::vector<entity_id> centre_tiles2;
    for (const auto& [mid, mc] : wm2.markets)
        if (mc.body == wm2.home_body)
            centre_tiles2.push_back(mc.centre_tile);
    std::sort(centre_tiles2.begin(), centre_tiles2.end());
    const bool market_determinism_ok = (centre_tiles == centre_tiles2);
    std::printf("  BL-096 R4 same seed reproduces the market map: %s\n",
                market_determinism_ok ? "PASS" : "FAIL");

    // --- BL-182 (corporate border foundation): every corp with holdings on its home
    // body is assigned an HQ that is one of its OWN buildings, sited on the home body,
    // and a positive influence_range; and the designation is deterministic across two
    // generations (same seed -> same HQ entity + range). Render-only chrome, but the
    // data model must be sound. ---
    int hq_checked = 0, hq_bad = 0;
    for (const auto& [cid, corp] : w.gen_charters)
    {
        if (corp.assets.empty()) continue;            // no holdings -> no HQ (fine)
        const entity_id hb = home_body_of(corp);
        if (hb == null_entity) continue;              // already reported by the stockpile audit
        ++hq_checked;
        const bool is_own = std::find(corp.assets.begin(), corp.assets.end(),
                                      corp.hq_building) != corp.assets.end();
        const auto bit = w.buildings.find(corp.hq_building);
        bool on_home = false;
        if (bit != w.buildings.end())
        {
            const auto t = w.tiles.find(bit->second.tile);
            on_home = (t != w.tiles.end() && t->second.body == hb);
        }
        if (corp.hq_building == null_entity || !is_own || !on_home
            || corp.influence_range <= 0.0f)
        {
            ++hq_bad;
            std::printf("  BAD: corp=%u hq=%u own=%d on_home=%d range=%.3f\n",
                        static_cast<unsigned>(cid), static_cast<unsigned>(corp.hq_building),
                        is_own ? 1 : 0, on_home ? 1 : 0, corp.influence_range);
        }
    }
    std::printf("Corp HQs: %d corps with home holdings, %d without a valid HQ\n",
                hq_checked, hq_bad);
    std::printf("  BL-182 R1 every home-holding corp has a valid HQ + positive range: %s\n",
                hq_bad == 0 ? "PASS" : "FAIL");

    // --- BL-257: the generated body catalogue -------------------------------
    // Names are coined per seed now, so this prints the catalogue (the sample a
    // reader judges the generator by) and asserts the two properties that make
    // it a system rather than a shuffle: no duplicates within a system, and the
    // same seed producing the same names run to run.
    bool body_names_unique = true, body_names_stable = true;
    {
        std::printf("Body catalogue (seed 0):\n");
        std::vector<entity_id> ids;
        for (const auto& [bid, bc] : w.bodies) ids.push_back(bid);
        std::sort(ids.begin(), ids.end());
        std::set<std::string> seen;
        for (const entity_id bid : ids)
        {
            const body_component& bc = w.bodies.at(bid);
            static const char* kType[5] = { "planet", "moon", "asteroid", "station", "star" };
            const int t = static_cast<int>(bc.type);
            std::printf("  %-24s %s%s\n", bc.name.c_str(),
                        (t >= 0 && t < 5) ? kType[t] : "?",
                        bid == w.home_body ? "  (home)" : "");
            if (!seen.insert(bc.name).second) body_names_unique = false;

            const auto it2 = w2.bodies.find(bid);
            if (it2 == w2.bodies.end() || it2->second.name != bc.name)
                body_names_stable = false;
        }
    }
    std::printf("  BL-257 R1 no two bodies share a name: %s\n",
                body_names_unique ? "PASS" : "FAIL");
    std::printf("  BL-257 R2 the same seed coins the same catalogue: %s\n",
                body_names_stable ? "PASS" : "FAIL");

    int hq_det_bad = 0;
    for (const auto& [cid, corp] : w.gen_charters)
    {
        const auto it2 = w2.gen_charters.find(cid);
        if (it2 == w2.gen_charters.end()) { ++hq_det_bad; continue; }
        if (corp.hq_building != it2->second.hq_building
            || corp.influence_range != it2->second.influence_range)
            ++hq_det_bad;
    }
    std::printf("  BL-182 R2 HQ designation identical across two generations (%d mismatched): %s\n",
                hq_det_bad, hq_det_bad == 0 ? "PASS" : "FAIL");

    // --- BL-283 (corp assets anchor in the home region): the spatial spread of
    // corporate holdings, as the region window sees it. Reported, not asserted —
    // a corp legitimately spills into a neighbouring region when its own has no
    // room, so the number to watch is the DISTRIBUTION, not a bound. ---
    {
        const auto  kbit = w.bodies.find(kepler);
        const int   kgw  = (kbit != w.bodies.end()) ? kbit->second.grid_width : 0;
        const generation_report::body_entry* kentry = nullptr;
        for (const auto& be : report.bodies)
            if (kbit != w.bodies.end() && be.name == kbit->second.name) { kentry = &be; break; }

        int corps_measured = 0, holdings_total = 0, regions_spanned_total = 0;
        int single_region_corps = 0;
        long long spread_sum = 0; // summed max grid separation within a corp's holdings
        std::map<std::pair<terrain_substrate, terrain_cover>, int> holding_comp;
        for (const auto& [cid, corp] : w.gen_charters)
        {
            std::set<int> provs;
            std::vector<std::pair<int,int>> pts;
            for (entity_id aid : corp.assets)
            {
                const auto b = w.buildings.find(aid);
                if (b == w.buildings.end()) continue;
                const auto t = w.tiles.find(b->second.tile);
                if (t == w.tiles.end() || t->second.body != kepler) continue;
                ++holdings_total;
                holding_comp[{ t->second.substrate, t->second.cover }]++;
                pts.emplace_back(t->second.grid_x, t->second.grid_y);
                if (kentry && kgw > 0)
                {
                    const int pi = nearest_region(kentry->settlement,
                                                    t->second.grid_x, t->second.grid_y, kgw);
                    if (pi >= 0) provs.insert(pi);
                }
            }
            if (pts.empty()) continue;
            ++corps_measured;
            regions_spanned_total += static_cast<int>(provs.size());
            if (provs.size() <= 1) ++single_region_corps;
            long long worst = 0;
            for (std::size_t i = 0; i < pts.size(); ++i)
                for (std::size_t j = i + 1; j < pts.size(); ++j)
                {
                    int dc = std::abs(pts[i].first - pts[j].first);
                    if (kgw > 0 && dc > kgw / 2) dc = kgw - dc;
                    const int dr = std::abs(pts[i].second - pts[j].second);
                    worst = std::max<long long>(worst, dc + dr);
                }
            spread_sum += worst;
            (void)cid;
        }
        std::printf("Corp holdings spatial spread: %d corps, %d holdings, "
                    "%.2f regions/corp (%d of %d single-region), "
                    "mean max separation %.1f tiles\n",
                    corps_measured, holdings_total,
                    corps_measured ? static_cast<double>(regions_spanned_total) / corps_measured : 0.0,
                    single_region_corps, corps_measured,
                    corps_measured ? static_cast<double>(spread_sum) / corps_measured : 0.0);
        std::printf("  holdings by terrain (substrate/cover):");
        for (const auto& [c, n] : holding_comp)
            std::printf(" s%d/c%d=%d", static_cast<int>(c.first), static_cast<int>(c.second), n);
        std::printf("\n");
    }

    // --- BL-284 (territorial fragmentation, measured): exclaves per nation across
    // a seed sweep, each attributed to the settlement sim (emergent) or to Pass 2b's
    // orphan-island cleanup (authored). The emergent share is the headline — it is
    // the number BL-218's argument for the expensive path predicted. ---
    frag_stats frag_total;
    int        strait_illegal = 0;
    {
        std::printf("Territorial fragmentation (BL-284) — exclaves by producer:\n");
        constexpr uint32_t sweep_seeds[] = { 0u, 1u, 2u, 3u, 4u, 5u };
        for (uint32_t s : sweep_seeds)
        {
            frag_stats   fs;
            strait_stats st, nh;
            int          ce = 0, nh_nations = 0;
            bool         closing_ran = false;
            std::string  violation;
            {
                // Built with a fixture so the closing rounds' own cession
                // count is readable: these worlds record no playback, so the
                // events `count_cessions` reads are not kept here.
                world_params          wp; wp.seed = s;
                generation_report     rp;
                era_minus_one_fixture fx{};
                const world sw = make_hard_coded_world(no_prehistory(wp), &rp, world_gen_config{}, nullptr, nullptr, &fx);
                fs = measure_fragmentation(sw, rp);
                st = audit_strait_rule(sw, rp);
                ce = fx.industrialisation_state.closing_cessions;
                violation = rp.handoff_violation;
                int realms = 0;
                for (const auto& be : rp.bodies)
                    if (be.is_homeworld)
                        for (const int32_t p : be.nation_polity) realms += p >= 0 ? 1 : 0;
                closing_ran = fx.industrialisation_state.closing_predicted >= 0;
                std::printf("    no-prehistory seed %u: %d nation(s) from a realm, closing rounds %s\n", s, realms,
                            closing_ran ? "ran" : "did not run");
                nh = audit_no_history(sw, s, nh_nations);
            }
            // THE STRAIT RULE WHERE NO HISTORY RAN (settlement anchors, no
            // realm, no closing rounds): no illegal exclave, no capital off
            // its own nation's land (Pass 5 seats a nation on a seed inside its
            // territory, so the count is a check: 0), no handoff violation;
            // and IF closing rounds ever run here, they must have ceded.
            strait_illegal += st.illegal + st.capital_off + (violation.empty() ? 0 : 1)
                            + ((closing_ran && ce == 0) ? 1 : 0);
            print_strait("no-prehistory", s, st, ce);
            if (!violation.empty()) std::printf("    VIOLATION: %s\n", violation.c_str());
            // AND THE RANDOM-PLACEMENT PATH over the same ground: Pass 2e as
            // the post-pass, then the floor at its real minimum, then the
            // repair after it.
            strait_illegal += nh.illegal + nh.capital_off;
            print_strait("no-history", s, nh, 0);
            std::printf("    no-history: %d nations from random seeds\n", nh_nations);
            std::printf("  seed %u: %d nations, %d with an exclave | emergent %d (%d tiles), "
                        "Pass 2b %d (%d tiles) | %d landmasses, %d seedless\n",
                        s, fs.nations, fs.nations_with_exclave,
                        fs.exclaves_emergent, fs.tiles_emergent,
                        fs.exclaves_pass2b, fs.tiles_pass2b,
                        fs.landmasses, fs.landmasses_seedless);
            frag_total.nations              += fs.nations;
            frag_total.nations_with_exclave += fs.nations_with_exclave;
            frag_total.exclaves_emergent    += fs.exclaves_emergent;
            frag_total.exclaves_pass2b      += fs.exclaves_pass2b;
            frag_total.tiles_emergent       += fs.tiles_emergent;
            frag_total.tiles_pass2b         += fs.tiles_pass2b;
            frag_total.landmasses           += fs.landmasses;
            frag_total.landmasses_seedless  += fs.landmasses_seedless;
        }
        const int total_exclaves = frag_total.exclaves_emergent + frag_total.exclaves_pass2b;
        const double emergent_share = total_exclaves
            ? 100.0 * frag_total.exclaves_emergent / total_exclaves : 0.0;
        std::printf("  sweep total: %d exclaves — %d emergent, %d Pass 2b "
                    "(emergent share %.1f%% of exclaves, %d of %d tiles)\n",
                    total_exclaves, frag_total.exclaves_emergent, frag_total.exclaves_pass2b,
                    emergent_share, frag_total.tiles_emergent,
                    frag_total.tiles_emergent + frag_total.tiles_pass2b);
    }
    // BL-1310 REPLACES BL-284's R1 (NATION_GENERATION.md § Territorial
    // fragmentation, measured; § Pass 2e). The old bar asserted the sim
    // produces exclaves at all; the strait rule now cedes every one, so the
    // finished world is asserted to hold NO exclave outside the rule save the
    // remote island: on six no-history worlds twice over (settlement anchors,
    // random placement), and on one world with its history, where the
    // closing rounds must also have ceded (the sim still produces the
    // fragmentation the rule resolves) with the frozen cores intact. Library
    // mode (`--seeds`) asserts the same on the app's data layer, natural and
    // targeted, plus events == steps.
    // ONE WORLD WITH ITS HISTORY (the review's gap): `no_prehistory` worlds
    // hold no realm and run no closing rounds, so the fragmentation the
    // history produces is read here, on the shipped arc's seed 0: the closing
    // rounds ran, they ceded (recorded as steps), and the rule holds.
    {
        world_params          wp; wp.seed = 0;
        generation_report     rp;
        era_minus_one_fixture fx{};
        const world sw = make_hard_coded_world(wp, &rp, world_gen_config{}, nullptr, nullptr, &fx);
        const strait_stats st = audit_strait_rule(sw, rp);
        const history_sim_state& hs = fx.industrialisation_state;
        const bool ok = hs.closing_predicted >= 0 && hs.closing_cessions > 0 && st.illegal == 0
                     && st.capital_off == 0 && rp.handoff_violation.empty() && hs.closing_core_tiles_lost == 0;
        print_strait("history", 0, st, hs.closing_cessions);
        std::printf("    history seed 0: closing rounds %s, cessions %d, core tiles lost %d%s%s : %s\n",
                    hs.closing_predicted >= 0 ? "ran" : "did NOT run", hs.closing_cessions,
                    hs.closing_core_tiles_lost, rp.handoff_violation.empty() ? "" : " | VIOLATION: ",
                    rp.handoff_violation.c_str(), ok ? "PASS" : "FAIL");
        strait_illegal += ok ? 0 : 1;
    }
    const bool frag_ok = strait_illegal == 0;
    std::printf("  BL-1310 R1 strait rule across the 6-seed sweep, history and no-history worlds "
                "(saw %d): %s\n",
                strait_illegal, frag_ok ? "PASS" : "FAIL");

    // --- BL-612 (urban ground stamped): every centre carries an urban
    // footprint; urban ground exists only where centres are. The stamp is the
    // centre's own tile plus hex neighbours, so wrapped-Chebyshev distance <= 1
    // is the (slightly wider, safe) envelope; the count is bracketed by the
    // distinct centre tiles below and the sum of k_urban_footprint_tiles above
    // (coast cuts short, shared tiles stamp once). ---
    bool urban_ok = true;
    {
        int centre_tiles = 0, urban_tiles = 0, footprint_budget = 0;
        int centres_not_urban = 0, urban_orphans = 0;
        std::set<entity_id> centre_tile_set;
        for (const auto& [cid, pcc] : w.population_centres)
        {
            const auto tit = w.population_centre_tile.find(cid);
            if (tit == w.population_centre_tile.end())
                continue;
            const entity_id ctile = tit->second;
            centre_tile_set.insert(ctile);
            ++centre_tiles;
            const int s = std::clamp(pcc.scale, 1, 5);
            footprint_budget += k_urban_footprint_tiles[s - 1];
            const auto lu = w.land_use.find(ctile);
            if (lu == w.land_use.end() || lu->second.use != land_use_component::type::urban)
                ++centres_not_urban;
        }
        for (const auto& [tid, luc] : w.land_use)
        {
            if (luc.use != land_use_component::type::urban)
                continue;
            ++urban_tiles;
            const auto tit = w.tiles.find(tid);
            if (tit == w.tiles.end()) { ++urban_orphans; continue; }
            const tile_component& tc = tit->second;
            const int width = w.bodies.at(tc.body).grid_width;
            bool near_centre = false;
            for (const entity_id ctile : centre_tile_set)
            {
                const auto cit = w.tiles.find(ctile);
                if (cit == w.tiles.end() || cit->second.body != tc.body)
                    continue;
                const int rdx = std::abs(cit->second.grid_x - tc.grid_x);
                const int dx = std::min(rdx, width - rdx);
                const int dy = std::abs(cit->second.grid_y - tc.grid_y);
                if (std::max(dx, dy) <= 1) { near_centre = true; break; }
            }
            if (!near_centre)
                ++urban_orphans;
        }
        const bool bracketed = urban_tiles >= static_cast<int>(centre_tile_set.size()) && urban_tiles <= footprint_budget;
        urban_ok = centres_not_urban == 0 && urban_orphans == 0 && bracketed && centre_tiles > 0;
        std::printf("\nBL-612 urban stamp: centres=%d urban=%d budget=%d "
                    "(unstamped centres %d, orphan urban %d, bracketed %s) : %s\n",
                    centre_tiles, urban_tiles, footprint_budget,
                    centres_not_urban, urban_orphans, bracketed ? "yes" : "no",
                    urban_ok ? "PASS" : "FAIL");
    }

    return (urban_ok && fw_frac >= 3.0f && bad == 0 && seed_bad == 0 && seam_bad == 0
            && unclaimed_land == 0 && holdings_bad == 0 && stockpile_ok
            && absent == 0 && ordering_ok
            && floor_ok && variance_ok && count_ok
            && market_count_ok && cross_nation_ok && market_determinism_ok
            && hq_bad == 0 && hq_det_bad == 0 && landform_absent == 0
            && frag_ok
            && body_names_unique && body_names_stable) ? 0 : 1;
}
