// ---------------------------------------------------------------------------
// plants_probe — BL-1232 (power plants per grid), the diagnosis
// ---------------------------------------------------------------------------
// QUESTION. power_grid_probe (BL-1230) found 156 grids for 106 markets on five
// seeds, only 6-9 of them carrying a live generator, and wired buildings on the
// generator-less grids holding 16.6% of power need at handoff. WHY does
// generation place so few plants, and why does the scorer not fill the gap?
// Per grid this reads: power need, generators (live / under construction /
// decommissioned, output at the nominal run), the grid's size in provinces, and
// for every generator-less grid with need, the candidate causes:
//   (a) BODY SIZING — the charter walk's gap test is body-wide
//       (body_upkeep_demand vs accumulate_body_production): if the body's
//       generators already cover the body's need, the walk saw power as met;
//   (b) SITE — no tile on the grid admits a processing facility for power
//       (can_place_in_world), or no FUEL route (no coal/petroleum deposit,
//       extraction site or shelf on the grid);
//   (c) SCORER — the corp AI sites a processor only on a background corp's OWN
//       non-processor tile and prices it at that tile's market: count such
//       slots on the grid, and whether any plant appeared there after handoff;
//   (d) a generator placed on the grid but under construction / decommissioned.
// Pure reader: it never writes to the world beyond the ordinary ticks.
//
// THE WORLD. Seated as power_grid_probe seats it: build_app_start_world, the
// 12-tick settle, seat_player_corporation, then PLAY ticks through
// run_settle_tick. Read at handoff and ticks 10 / 50 / 100.
//
// Usage (repo root): build_gen/verify/plants_probe.exe [--seeds a,b]
// Build:  ./tools/verify/build_lua_harness.sh plants_probe
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp" // BL-1232 review: body_power_grid_gap, one_power_plant_output
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/spawn_seat.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr std::size_t k_power = static_cast<std::size_t>(resource_type::power);
constexpr std::size_t k_coal  = static_cast<std::size_t>(resource_type::coal);
constexpr std::size_t k_oil   = static_cast<std::size_t>(resource_type::petroleum);

struct grid_row
{
    std::uint32_t id = 0;
    entity_id body = null_entity;
    int provinces = 0;
    int markets = 0;                 // market centres standing on the grid
    double need = 0; long drawers = 0;
    long gen_live = 0; double out_live = 0;
    long gen_building = 0, gen_decom = 0;
    long gen_player = 0;             // generators (any state) the seated player corp owns
    long building_ticks_left = 0;    // summed ticks_remaining over the plants under construction
    bool fuel_deposit = false, fuel_site = false, fuel_shelf = false;
    long placeable_tiles = 0;        // tiles admitting a processing facility for power
    long scorer_slots = 0;           // background corp tiles with no processor (corp_ai siting)
    double price_ratio = 0;          // mean power price / base over the grid's markets
};

struct snapshot
{
    std::map<std::uint32_t, grid_row> grids;
    std::map<entity_id, std::pair<double, double>> body_need_out;   // body -> (need, live output)
    std::set<entity_id> generators;                                  // live generator ids
    double need_all = 0, need_dark = 0;
};

snapshot read(world& w, const recipe_registry& reg)
{
    snapshot S;
    const building_upkeep_params& up = reg.building_upkeep();
    const float batches = reg.economics(building_type::processing_facility).base_rate * 0.5f;
    const auto& pgrid = province_power_grid(w);

    // provinces per grid, and the body each grid sits on
    for (const province& p : w.provinces.provinces)
    {
        const auto it = pgrid.find(p.id);
        if (it == pgrid.end()) continue;
        grid_row& g = S.grids[it->second];
        g.id = it->second;
        ++g.provinces;
        if (g.body == null_entity && !p.tiles.empty())
            if (const auto tit = w.tiles.find(p.tiles.front()); tit != w.tiles.end()) g.body = tit->second.body;
        for (const entity_id t : p.tiles)
        {
            const auto tit = w.tiles.find(t);
            if (tit == w.tiles.end()) continue;
            const tile_component& tc = tit->second;
            if (tc.resource_deposit[k_coal] > 0.0f || tc.resource_deposit[k_oil] > 0.0f) g.fuel_deposit = true;
            if (placement_rules::can_place_in_world(w, t, building_type::processing_facility, resource_type::power))
                ++g.placeable_tiles;
        }
    }
    // markets on each grid
    std::vector<entity_id> mids;
    for (const auto& [mid, mc] : w.markets) { (void)mc; mids.push_back(mid); }
    std::sort(mids.begin(), mids.end());
    std::map<std::uint32_t, int> priced;
    for (const entity_id mid : mids)
    {
        const market_component& mc = w.markets.at(mid);
        if (mc.centre_tile == null_entity) continue;
        const std::uint32_t g = tile_power_grid(w, mc.centre_tile);
        if (g == 0) continue;
        grid_row& r = S.grids[g];
        ++r.markets;
        if (mc.inventory[k_coal] > 0.0f || mc.inventory[k_oil] > 0.0f) r.fuel_shelf = true;
        if (mc.base_price[k_power] > 0.0f) { r.price_ratio += mc.price[k_power] / mc.base_price[k_power]; ++priced[g]; }
    }
    for (auto& [g, n] : priced) if (n > 0) S.grids[g].price_ratio /= n;

    // ownership: building -> is it a background corp's?
    std::vector<entity_id> cids;
    for (const auto& [cid, cc] : w.corporations) { (void)cc; cids.push_back(cid); }
    std::sort(cids.begin(), cids.end());
    std::map<entity_id, bool> bg_owned;
    std::set<entity_id> player_owned;
    std::map<entity_id, std::set<entity_id>> corp_tiles, corp_proc_tiles;
    for (const entity_id cid : cids)
    {
        const corporation_component& cc = w.corporations.at(cid);
        for (const entity_id bid : cc.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end()) continue;
            if (!cc.is_player) { corp_tiles[cid].insert(bit->second.tile); bg_owned[bid] = true; }
            else player_owned.insert(bid);
            if (bit->second.type == building_type::processing_facility) corp_proc_tiles[cid].insert(bit->second.tile);
        }
    }
    for (const entity_id cid : cids)
        for (const entity_id t : corp_tiles[cid])
            if (!corp_proc_tiles[cid].count(t))
                if (const std::uint32_t g = tile_power_grid(w, t); g != 0) ++S.grids[g].scorer_slots;

    std::vector<entity_id> bids;
    for (const auto& [bid, b] : w.buildings) { (void)b; bids.push_back(bid); }
    std::sort(bids.begin(), bids.end());
    for (const entity_id bid : bids)
    {
        const building_component& b = w.buildings.at(bid);
        const auto tit = w.tiles.find(b.tile);
        const entity_id body = (tit != w.tiles.end()) ? tit->second.body : null_entity;
        const std::uint32_t pg = tile_power_grid(w, b.tile);
        if (b.type == building_type::processing_facility)
            if (const recipe* rc = reg.get_recipe(b.recipe); rc && rc->outputs[k_power] > 0.0f)
            {
                if (pg && player_owned.count(bid)) ++S.grids[pg].gen_player;
                if (b.decommissioned) { if (pg) ++S.grids[pg].gen_decom; continue; }
                if (b.ticks_remaining > 0) { if (pg) { ++S.grids[pg].gen_building; S.grids[pg].building_ticks_left += b.ticks_remaining; } continue; }
                const double o = batches * rc->outputs[k_power];
                S.body_need_out[body].second += o;
                S.generators.insert(bid);
                if (pg) { ++S.grids[pg].gen_live; S.grids[pg].out_live += o; }
            }
        if (b.type == building_type::extraction_site && !b.decommissioned && b.ticks_remaining <= 0
            && (b.target_resource == resource_type::coal || b.target_resource == resource_type::petroleum) && pg)
            S.grids[pg].fuel_site = true;
        if (b.ticks_remaining > 0 || b.decommissioned) continue;
        const float need = building_upkeep_goods(up, b.type, reg.era())[k_power];
        if (!(need > 0.0f)) continue;
        S.need_all += need;
        S.body_need_out[body].first += need;
        if (pg == 0) { S.need_dark += need; continue; }
        S.grids[pg].need += need; ++S.grids[pg].drawers;
    }
    (void)bg_owned;
    return S;
}

double pc(double a, double b) { return b > 0 ? 100.0 * a / b : 0.0; }

struct tally
{
    long grids = 0, with_need = 0, with_gen = 0, nogen_need = 0;
    double need = 0, need_nogen = 0, need_all = 0, need_dark = 0;
    // size classes over grids with need: [0] 1 province, [1] 2-4, [2] 5+
    long sz_grids[3] = {}, sz_nogen[3] = {};
    double sz_need[3] = {}, sz_need_nogen[3] = {};
    // causes over generator-less grids with need (need-weighted and counted)
    long c_a = 0, c_b_place = 0, c_b_fuel = 0, c_d = 0, c_noslot = 0, c_slot_fuel = 0;
    double n_a = 0, n_b_place = 0, n_b_fuel = 0, n_d = 0, n_noslot = 0, n_slot_fuel = 0;
    double price_nogen = 0; long price_n = 0;
    long gens = 0, new_gens = 0;
    long bodies = 0, bodies_met = 0;
};

int size_class(int p) { return p <= 1 ? 0 : (p <= 4 ? 1 : 2); }

tally tally_of(const snapshot& S, const snapshot* base)
{
    tally T;
    T.need_all = S.need_all; T.need_dark = S.need_dark;
    T.gens = static_cast<long>(S.generators.size());
    if (base) for (const entity_id g : S.generators) if (!base->generators.count(g)) ++T.new_gens;
    std::set<entity_id> met_bodies;
    for (const auto& [body, no] : S.body_need_out)
    {
        if (!(no.first > 0)) continue;
        ++T.bodies;
        if (no.second >= no.first) { ++T.bodies_met; met_bodies.insert(body); }
    }
    for (const auto& [id, g] : S.grids)
    {
        (void)id;
        ++T.grids;
        if (g.gen_live > 0) ++T.with_gen;
        if (!(g.need > 0)) continue;
        ++T.with_need; T.need += g.need;
        const int sc = size_class(g.provinces);
        ++T.sz_grids[sc]; T.sz_need[sc] += g.need;
        if (g.gen_live > 0) continue;
        ++T.nogen_need; T.need_nogen += g.need;
        ++T.sz_nogen[sc]; T.sz_need_nogen[sc] += g.need;
        if (met_bodies.count(g.body)) { ++T.c_a; T.n_a += g.need; }
        if (g.placeable_tiles == 0) { ++T.c_b_place; T.n_b_place += g.need; }
        if (!g.fuel_deposit && !g.fuel_site && !g.fuel_shelf) { ++T.c_b_fuel; T.n_b_fuel += g.need; }
        if (g.gen_building + g.gen_decom > 0) { ++T.c_d; T.n_d += g.need; }
        if (g.scorer_slots == 0) { ++T.c_noslot; T.n_noslot += g.need; }
        else if (g.fuel_site || g.fuel_shelf) { ++T.c_slot_fuel; T.n_slot_fuel += g.need; }
        if (g.markets > 0) { T.price_nogen += g.price_ratio; ++T.price_n; }
    }
    return T;
}

void add(tally& a, const tally& b)
{
    a.grids += b.grids; a.with_need += b.with_need; a.with_gen += b.with_gen; a.nogen_need += b.nogen_need;
    a.need += b.need; a.need_nogen += b.need_nogen; a.need_all += b.need_all; a.need_dark += b.need_dark;
    for (int i = 0; i < 3; ++i)
    { a.sz_grids[i] += b.sz_grids[i]; a.sz_nogen[i] += b.sz_nogen[i]; a.sz_need[i] += b.sz_need[i]; a.sz_need_nogen[i] += b.sz_need_nogen[i]; }
    a.c_a += b.c_a; a.c_b_place += b.c_b_place; a.c_b_fuel += b.c_b_fuel; a.c_d += b.c_d; a.c_noslot += b.c_noslot; a.c_slot_fuel += b.c_slot_fuel;
    a.n_a += b.n_a; a.n_b_place += b.n_b_place; a.n_b_fuel += b.n_b_fuel; a.n_d += b.n_d; a.n_noslot += b.n_noslot; a.n_slot_fuel += b.n_slot_fuel;
    a.price_nogen += b.price_nogen; a.price_n += b.price_n;
    a.gens += b.gens; a.new_gens += b.new_gens; a.bodies += b.bodies; a.bodies_met += b.bodies_met;
}

void print(const char* tag, const tally& T)
{
    std::printf("  %-7s grids %ld (with need %ld, with a live generator %ld) | generators %ld (new since handoff %ld)"
                " | bodies with need %ld, body output >= body need %ld\n",
                tag, T.grids, T.with_need, T.with_gen, T.gens, T.new_gens, T.bodies, T.bodies_met);
    std::printf("          need %.1f: dark %.1f%% | wired on a generator-less grid %.1f%% (%ld grids)\n",
                T.need_all, pc(T.need_dark, T.need_all), pc(T.need_nogen, T.need_all), T.nogen_need);
    static const char* sz[3] = {"1 prov", "2-4 prov", "5+ prov"};
    for (int i = 0; i < 3; ++i)
        std::printf("          %-8s grids with need %3ld (%.1f%% of wired need) | generator-less %3ld (%.1f%% of all need)\n",
                    sz[i], T.sz_grids[i], pc(T.sz_need[i], T.need), T.sz_nogen[i], pc(T.sz_need_nogen[i], T.need_all));
    std::printf("          causes over generator-less grids (count, %% of their need; NOT exclusive):\n"
                "            (a) body output already >= body need .... %3ld  %5.1f%%\n"
                "            (b) no tile admits a power plant ......... %3ld  %5.1f%%\n"
                "            (b) no fuel deposit/site/shelf on grid ... %3ld  %5.1f%%\n"
                "            (c) no background-corp siting slot ....... %3ld  %5.1f%%\n"
                "            (c) slot AND fuel present, still no plant  %3ld  %5.1f%%\n"
                "            (d) plant building / decommissioned ...... %3ld  %5.1f%%\n"
                "            mean power price/base on their markets %.2f (%ld grids with a market)\n",
                T.c_a, pc(T.n_a, T.need_nogen), T.c_b_place, pc(T.n_b_place, T.need_nogen),
                T.c_b_fuel, pc(T.n_b_fuel, T.need_nogen), T.c_noslot, pc(T.n_noslot, T.need_nogen),
                T.c_slot_fuel, pc(T.n_slot_fuel, T.need_nogen), T.c_d, pc(T.n_d, T.need_nogen),
                T.price_n ? T.price_nogen / T.price_n : 0.0, T.price_n);
}

void print_bodies(const snapshot& S)
{
    for (const auto& [body, no] : S.body_need_out)
        if (no.first > 0)
            std::printf("          body %llu: need %.1f, live generator output %.1f (%s)\n",
                        static_cast<unsigned long long>(body), no.first, no.second,
                        no.second >= no.first ? "MET at body grain" : "short");
}

void print_gen_grids(const snapshot& S)
{
    for (const auto& [id, g] : S.grids)
        if (g.gen_live > 0 || g.gen_building > 0 || g.gen_decom > 0)
            std::printf("          grid %u: %d prov, %d markets, need %.1f, generators %ld live (out %.1f) %ld building"
                        " (mean ticks left %.1f) %ld decommissioned, %ld player-owned\n",
                        id, g.provinces, g.markets, g.need, g.gen_live, g.out_live, g.gen_building,
                        g.gen_building ? static_cast<double>(g.building_ticks_left) / g.gen_building : 0.0,
                        g.gen_decom, g.gen_player);
}

} // namespace

// BL-1232 review round: a generator FEEDS the grid its market's centre is on
// (tile_feed_power_grid), which need not be its own tile's grid. After
// generation, count the generators whose two grids differ, and per grid print
// the need, the output its own tiles hold, the output that actually FEEDS it,
// and the gap generation's measure reads (body_power_grid_gap, feed-keyed).
void print_feed_check(world& w, const recipe_registry& reg)
{
    const float batches = reg.economics(building_type::processing_facility).base_rate * 0.5f;
    const float plant = one_power_plant_output(reg);
    std::vector<entity_id> bids;
    for (const auto& [bid, b] : w.buildings) { (void)b; bids.push_back(bid); }
    std::sort(bids.begin(), bids.end());
    std::map<std::uint32_t, std::array<double, 3>> g;   // need, output on tile grid, output feeding
    std::set<entity_id> bodies;
    long gens = 0, differ = 0, dark_feed = 0;
    const building_upkeep_params& up = reg.building_upkeep();
    for (const entity_id bid : bids)
    {
        const building_component& b = w.buildings.at(bid);
        if (b.decommissioned) continue;
        if (const auto tit = w.tiles.find(b.tile); tit != w.tiles.end()) bodies.insert(tit->second.body);
        const std::uint32_t tg = tile_power_grid(w, b.tile);
        if (b.ticks_remaining <= 0 && tg != 0)
            g[tg][0] += building_upkeep_goods(up, b.type, reg.era())[k_power];
        if (b.type != building_type::processing_facility) continue;
        const recipe* rc = reg.get_recipe(b.recipe);
        if (!rc || !(rc->outputs[k_power] > 0.0f)) continue;
        ++gens;
        const std::uint32_t fg = tile_feed_power_grid(w, b.tile);
        const double o = batches * rc->outputs[k_power];
        if (tg != 0) g[tg][1] += o;
        if (fg != 0) g[fg][2] += o; else ++dark_feed;
        if (tg != fg) ++differ;
    }
    std::printf("  feed check: generators %ld, tile grid != market-centre grid %ld, market centre dark %ld\n",
                gens, differ, dark_feed);
    for (const auto& [id, a] : g)
        if (a[1] > 0.0 || a[2] > 0.0 || a[0] >= 0.5 * plant)
            std::printf("          grid %u: need %.1f | output on its tiles %.1f | output FEEDING it %.1f | gap read %.1f%s\n",
                        id, a[0], a[1], a[2], (a[0] > a[2] && a[0] >= 0.5 * plant) ? a[0] - a[2] : 0.0,
                        (a[1] != a[2]) ? "  <- tile and feed differ" : "");
    double measure = 0.0;
    for (const entity_id body : bodies)
    {
        std::set<std::uint32_t> sg;
        measure += body_power_grid_gap(w, reg, body, plant, sg);
    }
    std::printf("          body_power_grid_gap summed over bodies: %.1f\n", measure);
}

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds = {0, 43, 10, 28, 38};
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc)
        {
            seeds.clear();
            std::string s = argv[++i];
            std::size_t a = 0;
            while (a <= s.size())
            {
                const std::size_t b = s.find(',', a);
                const std::string tok = s.substr(a, b == std::string::npos ? std::string::npos : b - a);
                if (!tok.empty()) seeds.push_back(static_cast<std::uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
                if (b == std::string::npos) break;
                a = b + 1;
            }
        }
        else { std::fprintf(stderr, "usage: plants_probe [--seeds a,b]\n"); return 2; }
    }
    std::printf("plants_probe - BL-1232: why so few grids carry a generator\n");

    const int checkpoints[] = {0, 10, 50, 100};
    tally pooled[4];
    for (const std::uint32_t seed : seeds)
    {
        lua_state lua;
        world_params wp;
        wp.seed = seed;
        auto start = std::make_unique<app_start_world>();
        try { build_app_start_world(lua, wp, *start); }
        catch (const std::exception& e) { std::printf("seed %u: build threw %s\n", seed, e.what()); continue; }
        world& w = start->w;
        const recipe_registry& reg = start->reg;
        std::printf("\n=== seed %u ===\n  generation (before the 12-tick settle): generators by grid\n", seed);
        {
            const snapshot sg = read(w, reg);
            print_gen_grids(sg);
            print_feed_check(w, reg);
        }
        for (int step = 0; step < k_campaign_settle_ticks; ++step) run_settle_tick(w, reg, step, 0, true);
        std::printf("  after the settle, before the seat:\n");
        {
            const snapshot ss = read(w, reg);
            print_gen_grids(ss);
        }
        seat_player_corporation(w, seed, start->land.search.winner_score);

        std::printf("  after settle + seat:\n");
        const snapshot s0 = read(w, reg);
        const tally t0 = tally_of(s0, nullptr);
        print("handoff", t0);
        print_bodies(s0);
        print_gen_grids(s0);
        add(pooled[0], t0);

        constexpr int k_days = 90;
        int k = 0;
        for (int ci = 1; ci < 4; ++ci)
        {
            for (; k < checkpoints[ci]; )
            {
                ++k;
                advance_orbits(w, static_cast<double>(k_days));
                advance_surveys(w, k_days);
                w.current_day_tick = k * k_days;
                (void)run_settle_tick(w, reg, k_campaign_settle_ticks + (k - 1), k * k_days, false, nullptr);
            }
            const snapshot s = read(w, reg);
            const tally t = tally_of(s, &s0);
            char tag[16];
            std::snprintf(tag, sizeof tag, "t%d", checkpoints[ci]);
            print(tag, t);
            print_bodies(s); print_gen_grids(s);
            add(pooled[ci], t);
        }
        std::fflush(stdout);
    }
    std::printf("\n================ POOLED over %zu seeds ================\n", seeds.size());
    for (int ci = 0; ci < 4; ++ci)
    {
        char tag[16];
        std::snprintf(tag, sizeof tag, ci == 0 ? "handoff" : "t%d", checkpoints[ci]);
        print(tag, pooled[ci]);
    }
    return 0;
}
