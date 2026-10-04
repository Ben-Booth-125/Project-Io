// steel_chain_probe — why does steel production on the home body collapse?
//
// WHY THIS EXISTS. On the shipped start (harness_params.hpp build_app_start_world,
// the world `ProjectIo --verify` plays), the steel listed across the home body's
// markets falls 234 -> 225 -> 75 -> 75 -> 0 inside ~9 economy ticks while body
// steel demand climbs and thousands of units sit on market shelves. This probe
// is the instrument that says WHY: per tick it follows every processor whose
// recipe makes steel on the home body — its run state, its idle reason, the
// availability of each input (own pool at its tile market + that market's
// shelf), where those inputs are produced, the owner's balance, and the reflex /
// strategic / firm-exit events that touch it — and one level up the chain, the
// producers of the inputs. It also counts every processor and extractor on the
// body by state and output good, so "is it steel or everything" has a number.
//
// A PURE READER. It steps the world through the same calls as
// run_app_validation_tick (harness_params.hpp — mirrored inline because the
// economy_report is needed between laps) and reads between them. It writes
// nothing the simulation reads.
//
// Build:  bash tools/verify/build_lua_harness.sh steel_chain_probe
//     or  cmd.exe //c "tools\verify\build_lua_harness.bat steel_chain_probe"
// Run (repo root):
//   build_gen/verify/steel_chain_probe.exe [--seeds 0,1,2] [--ticks 20] [--fast]
//        [--good steel] [--quiet] [--epoch Y]
//   --epoch  campaign epoch year (default: world_params' own, 1960). It names the
//            calendar only: the band comes from the history (BL-1101), and epoch
//            0 is retired (BL-1114). The first reading below predates both.
//   --fast   no pre-epoch history (NOT the shipped world; quick iteration only)
//   --good   follow another processed good's makers instead of steel
//   --quiet  suppress the per-maker rows; print the per-tick aggregates only
//   --no-breakdown  skip BL-1186's refusal breakdown (on by default)
//   --detail-ticks -1,0,10,12,20  ticks printing sample pairs + the market table (-1 = pre-settle, 12 = handoff)
//   --lp-scale X  COUNTERFACTUAL (not the shipped world): scale the passive-LP cap
//            per anchor by X from the first probed tick — BL-1071's reading of how
//            much shelf stock the cap, rather than the price rule, holds home
//
// FIRST READING (2026-09-24, seeds 0-2, shipped epoch 0 = ANCIENT band): there
// is no steel production to collapse. The home body starts with ZERO steel
// makers; the "listed" series is the corporations' OPENING steel pools being
// auctioned down (seed 0: 525 -> 300 -> 75 -> 0). The climbing demand is
// construction: the scorer's extraction sites for coal / petroleum / iron ore /
// copper / rare earths cost 20 steel each (the type default), every paused one
// sits in a market whose own steel shelf is EMPTY, and shelf stock is never
// hauled. The bloomeries it does start are capacity-stretched to 0.1/tick.
// Pre-BL-1003 (a185c0ff) shows the same absence. Read the `construction:` and
// `geography` lines first.
//
// BL-1071 READING (2026-09-24, seed 0, 20 ticks): with markets exporting their
// own shelves, steel moves but little — ~46 u over 20 ticks, while the same
// pass exports 30-49 cargoes a tick across every good and the passive-LP gate
// refuses 11-42 a tick. `--lp-scale 100` (a counterfactual) moves ~680 u and
// spreads steel from 13 to 16-17 shelves: the cap, taken first by corporations
// and by goods earlier in the walk, is the main brake on the shelf; the rest is
// the price rule (most shelf -> wanting pairs fail price_d > price_src x 1.05,
// the `shelf test` line). Read `market exports` and `shelf test` first.
//
// BL-1186 READING (2026-10-04, seeds 0/43/37, steel and coal, 21 ticks): the
// `[BL-1186]` block breaks every refused pair down by the first rule it fails,
// read at the real decision point (after run_economy_step, before
// dispatch_convoys), for the market's own shelf AND every corporation's pool.
// Unroutable is 100% "cheapest path crosses water, no port at both ends"; the
// price gate fails because the destination registers no demand on 3 ticks of
// 4 (a lockstep price cycle under the 2x fair-price ceiling with listings-only
// pricing), and on the 4th tick its same-landmass sources stand at the same
// price. Full write-up: docs/development/drafts/sprint-49-shipment-diagnosis.md.
//
// Ticks are econ steps 0..N-1 with the validation run's tick state (spectated,
// day tick 0) — the first 12 ARE the app's validation run; beyond that the probe
// keeps stepping the same way (a spectated continuation, not the seated game).

#include "scripting/lua_state.hpp"
#include "world/building_profit.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/resource_names.hpp"
#include "world/supply_system.hpp" // BL-1071: the shelf test (market_shelf_surplus, legs, room)
#include "world/world.hpp"
#include "harness_params.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

std::string rn(std::size_t r) { return resource_names::name_of(static_cast<resource_type>(r)); }

struct options
{
    std::vector<std::uint32_t> seeds{0};
    int  ticks = 20;
    bool fast  = false;
    bool quiet = false;
    long long epoch = -1;   ///< -1: world_params' own default (the shipped epoch)
    resource_type good = resource_type::steel;
    float lp_scale = 1.0f;  ///< --lp-scale: a counterfactual passive-LP cap (BL-1071 reading)
    bool breakdown = true;  ///< BL-1186: the per-refusal breakdown (--no-breakdown)
    std::set<int> detail_ticks{-1, 0, 10, 12, 20}; ///< ticks printing samples + market table (-1 = pre-settle; 12 = seat handoff)
};

/// Owner of every building, by a sorted corp walk (assets lists).
std::map<entity_id, entity_id> owners(const world& w)
{
    std::vector<entity_id> cids;
    for (const auto& kv : w.corporations)
        cids.push_back(kv.first);
    std::sort(cids.begin(), cids.end());
    std::map<entity_id, entity_id> out;
    for (const entity_id c : cids)
        for (const entity_id b : w.corporations.at(c).assets)
            out[b] = c;
    return out;
}

entity_id body_of_building(const world& w, const building_component& b)
{
    const auto it = w.tiles.find(b.tile);
    return it == w.tiles.end() ? null_entity : it->second.body;
}

/// Processor state, one word.
const char* proc_state(const building_component& b, const building_report* rep,
                       const recipe_registry& reg)
{
    if (b.ticks_remaining > 0)             return "BUILD";
    if (b.decommissioned)                  return "DECOM";
    if (reg.get_recipe(b.recipe) == nullptr) return "NORCP";
    if (rep == nullptr)                    return "NOREP";
    if (rep->active)                       return "RUN";
    if (rep->effective_workforce <= 0.0f)  return "NOLAB";
    if (rep->has_limiting)                 return "INPUT";   // coverage < t_idle
    return "IDLE?";
}

bool recipe_outputs(const recipe_registry& reg, uint16_t id, std::size_t r)
{
    const recipe* rc = reg.get_recipe(id);
    return rc != nullptr && rc->outputs[r] > 0.0f;
}

struct state_tally
{
    int n = 0, run = 0, input = 0, nolab = 0, decom = 0, build = 0, other = 0;
    float out = 0.0f;
    void add(const char* s, float q)
    {
        ++n;
        out += q;
        if (!std::strcmp(s, "RUN"))        ++run;
        else if (!std::strcmp(s, "INPUT")) ++input;
        else if (!std::strcmp(s, "NOLAB")) ++nolab;
        else if (!std::strcmp(s, "DECOM")) ++decom;
        else if (!std::strcmp(s, "BUILD")) ++build;
        else                               ++other;
    }
};

// ===========================================================================
// BL-1186 (goods cross markets) — THE REFUSAL BREAKDOWN. A pure reader: for
// every (source, destination) pair the next dispatch would weigh for good G,
// the FIRST rule it fails, split into the sub-case that fails it, with the
// numbers. Mirrors export_market_shelves (the market's own shelf) and
// dispatch_convoys (every corporation's pooled surplus) rule for rule
// (supply_system.cpp), and is called at the point the real pass decides —
// after run_economy_step, before dispatch_convoys — plus once at handoff
// (before the first econ step: the world build_app_start_world returns).
// Writes nothing the simulation reads (only the A* / flood caches warm).
// ===========================================================================

bool probe_tile_has_active_port(const world& w, entity_id tile)
{
    if (tile == null_entity)
        return false;
    for (const auto& [bid, bc] : w.buildings)
        if (bc.tile == tile && bc.type == building_type::port && bc.ticks_remaining <= 0 &&
            !bc.decommissioned)
            return true;
    return false;
}

bool probe_corp_has_launchpad_on(const world& w, const corporation_component& corp, entity_id body)
{
    for (entity_id a : corp.assets)
    {
        const auto bit = w.buildings.find(a);
        if (bit == w.buildings.end() || bit->second.type != building_type::launchpad)
            continue;
        const auto tit = w.tiles.find(bit->second.tile);
        if (tit != w.tiles.end() && tit->second.body == body)
            return true;
    }
    return false;
}

/// Land-only travel cost (no water cell entered) from every cell to a
/// destination tile, by raster index; cached per destination for one call.
struct land_cache
{
    std::map<entity_id, std::map<int, float>> by_dest;
    const std::map<int, float>& get(world& w, entity_id body, entity_id dest)
    {
        auto it = by_dest.find(dest);
        if (it != by_dest.end())
            return it->second;
        std::map<int, float> m;
        for (const auto& [idx, c] : bounded_cost_to_tile(w, body, dest, 1e29f, /*land_only=*/true))
            m[idx] = c;
        return by_dest.emplace(dest, std::move(m)).first->second;
    }
};

int raster_of(const world& w, entity_id tile)
{
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end())
        return -1;
    const auto bit = w.bodies.find(tit->second.body);
    if (bit == w.bodies.end())
        return -1;
    return tit->second.grid_x + tit->second.grid_y * bit->second.grid_width;
}

bool tile_is_water(const world& w, entity_id tile)
{
    const auto tit = w.tiles.find(tile);
    return tit != w.tiles.end() && is_water(tit->second.substrate);
}

struct refusal_tally
{
    long pairs = 0;
    // price gate (price_d <= price_src x (1 + margin))
    long gate = 0, gate_dest_below_src = 0, gate_in_margin_band = 0, gate_src_at_ceiling = 0,
         gate_both_at_ceiling = 0, gate_dest_untraded = 0, gate_dest_no_demand = 0;
    // leg
    long unroutable = 0, un_no_anchor = 0, un_no_path = 0, un_water_no_port = 0,
         un_water_port_src_only = 0, un_water_port_dst_only = 0, un_water_endpoint_is_water = 0,
         un_water_land_path_exists = 0, un_water_land_path_would_pay = 0, un_cross_body = 0,
         un_cross_body_no_pad = 0, un_nonfinite = 0;
    // margin
    long margin = 0;
    // room
    long no_room = 0, room_landed_over_ceiling = 0, room_dest_no_demand = 0,
         room_absorb_le_pending = 0, room_dest_shelf_alone_fills = 0;
    long go = 0;
    double go_qty = 0.0;
    // the haul vs price numbers, summed for means
    double un_land_haul_sum = 0.0, margin_haul_sum = 0.0, margin_gap_sum = 0.0;
    double gate_psrc_over_base_sum = 0.0, gate_pd_over_base_sum = 0.0;
};

void print_tally(const char* who, const char* good, const refusal_tally& t, long sources,
                 double surplus)
{
    std::printf("  [BL-1186] %s %s: %ld sources holding %.1f surplus, %ld pairs\n", who, good,
                sources, surplus, t.pairs);
    std::printf("    price<=gate %ld: dest below src %ld, dest within margin band %ld | src at "
                "ceiling %ld, BOTH at ceiling %ld, dest untraded(base 0) %ld, dest no demand %ld | "
                "mean p_src/base %.2f p_d/base %.2f\n",
                t.gate, t.gate_dest_below_src, t.gate_in_margin_band, t.gate_src_at_ceiling,
                t.gate_both_at_ceiling, t.gate_dest_untraded, t.gate_dest_no_demand,
                t.gate ? t.gate_psrc_over_base_sum / t.gate : 0.0,
                t.gate ? t.gate_pd_over_base_sum / t.gate : 0.0);
    std::printf("    unroutable %ld: no anchor tile %ld, no path %ld, cheapest path crosses water "
                "w/o port at both ends %ld (port src only %ld, dst only %ld; an endpoint tile IS "
                "water %ld; a LAND-ONLY path exists %ld, of which would clear the margin at "
                "land rate %ld; mean land-only haul/u %.2f), cross-body %ld (no launchpad %ld), "
                "non-finite %ld\n",
                t.unroutable, t.un_no_anchor, t.un_no_path, t.un_water_no_port,
                t.un_water_port_src_only, t.un_water_port_dst_only, t.un_water_endpoint_is_water,
                t.un_water_land_path_exists, t.un_water_land_path_would_pay,
                t.un_water_land_path_exists ? t.un_land_haul_sum / t.un_water_land_path_exists : 0.0,
                t.un_cross_body, t.un_cross_body_no_pad, t.un_nonfinite);
    std::printf("    net under margin %ld (mean haul/u %.2f vs mean gross gap p_d-p_src %.2f)\n",
                t.margin, t.margin ? t.margin_haul_sum / t.margin : 0.0,
                t.margin ? t.margin_gap_sum / t.margin : 0.0);
    std::printf("    no room %ld: landed >= ceiling %ld, dest demand 0 %ld, absorbable <= pending "
                "%ld (dest's own shelf alone >= absorbable %ld)\n",
                t.no_room, t.room_landed_over_ceiling, t.room_dest_no_demand,
                t.room_absorb_le_pending, t.room_dest_shelf_alone_fills);
    std::printf("    would send %ld pairs (sum min(surplus, room) %.1f u, before the passive-LP cap)\n",
                t.go, t.go_qty);
}

/// One (source -> destination) pair through the net-price rule. `origin` is
/// the haul's start tile; `shelf_term` adds the destination shelf to pending
/// (the market-export rule only). `corp` null = market export.
void classify_pair(world& w, const recipe_registry& reg, const logistics_nodes& nodes,
                   const std::vector<entity_id>& corp_sorted, reservation_memo& memo,
                   land_cache& lands, std::size_t G, entity_id corp, entity_id src_key,
                   entity_id src_body, entity_id origin, float p_src, float surplus,
                   entity_id d_id, bool shelf_term, refusal_tally& t, int* samples, int max_samples,
                   const char* who)
{
    const float margin = reg.dispatch_margin();
    const float gate   = p_src + margin * p_src;
    const market_component& dm = w.markets.at(d_id);
    const float p_d  = dispatch_market_price(dm, G);
    const float ceil = reg.price_band().ceil_mult;
    ++t.pairs;
    auto sample = [&](const char* why, const char* fmt, auto... args) {
        if (*samples >= max_samples)
            return;
        ++*samples;
        std::printf("      sample %s %s src m%u -> m%u: ", who, why, static_cast<unsigned>(src_key),
                    static_cast<unsigned>(d_id));
        std::printf(fmt, args...);
        std::printf("\n");
    };

    if (!(p_d > gate))
    {
        ++t.gate;
        const auto sit = w.markets.find(src_key);
        const float sbase = sit != w.markets.end() ? sit->second.base_price[G] : 0.0f;
        const bool src_ceil = sbase > 0.0f && p_src >= 0.999f * ceil * sbase;
        const bool dst_ceil = dm.base_price[G] > 0.0f && p_d >= 0.999f * ceil * dm.base_price[G];
        if (p_d < p_src) ++t.gate_dest_below_src; else ++t.gate_in_margin_band;
        if (src_ceil) ++t.gate_src_at_ceiling;
        if (src_ceil && dst_ceil) ++t.gate_both_at_ceiling;
        if (dm.base_price[G] <= 0.0f) ++t.gate_dest_untraded;
        if (dm.demand[G] <= 0.0f) ++t.gate_dest_no_demand;
        if (sbase > 0.0f) t.gate_psrc_over_base_sum += p_src / sbase;
        if (dm.base_price[G] > 0.0f) t.gate_pd_over_base_sum += p_d / dm.base_price[G];
        sample("GATE", "p_src %.2f (base %.2f) gate %.2f p_d %.2f (base %.2f) d.demand %.1f d.supply %.1f d.shelf %.1f",
               p_src, sbase, gate, p_d, dm.base_price[G], dm.demand[G], dm.supply[G], dm.inventory[G]);
        return;
    }

    convoy_leg leg;
    if (corp == null_entity)
        leg = price_market_export_leg(w, reg, nodes, src_key, d_id, 1.0f);
    else
        leg = price_convoy_leg(w, reg, nodes, corp, src_key, d_id, G, 1.0f,
                               reg.logistics_cost(convoy_mode::space),
                               dm.body == src_body ? &origin : nullptr);
    if (!leg.viable)
    {
        ++t.unroutable;
        if (dm.body != src_body)
        {
            ++t.un_cross_body;
            if (corp != null_entity &&
                !probe_corp_has_launchpad_on(w, w.corporations.at(corp), src_body))
                ++t.un_cross_body_no_pad;
            return;
        }
        const entity_id dc = dm.centre_tile;
        if (origin == null_entity || dc == null_entity) { ++t.un_no_anchor; return; }
        const logistics_path& path = intra_body_path(w, src_body, origin, dc);
        if (!path.reachable) { ++t.un_no_path; return; }
        if (path.crosses_ocean)
        {
            const float path_cost = path.cost;
            const std::size_t path_len = path.tiles.size();
            ++t.un_water_no_port;
            const bool po = probe_tile_has_active_port(w, origin);
            const bool pd = probe_tile_has_active_port(w, dc);
            if (po && !pd) ++t.un_water_port_src_only;
            if (pd && !po) ++t.un_water_port_dst_only;
            const bool ow = tile_is_water(w, origin), dw = tile_is_water(w, dc);
            if (ow || dw) ++t.un_water_endpoint_is_water;
            const auto& land = lands.get(w, src_body, dc);
            const auto lit   = land.find(raster_of(w, origin));
            float land_haul  = -1.0f;
            if (lit != land.end())
            {
                ++t.un_water_land_path_exists;
                land_haul = reg.logistics_cost(convoy_mode::land) * lit->second; // no node discount
                t.un_land_haul_sum += land_haul;
                if (p_d - land_haul - p_src > margin * p_src)
                    ++t.un_water_land_path_would_pay;
            }
            sample("WATER", "cheapest path cost %.1f (%zu tiles) crosses water; port src %d dst %d; "
                   "origin water %d dest-centre water %d; land-only path cost %.1f -> haul/u %.2f "
                   "(p_src %.2f p_d %.2f)",
                   path_cost, path_len, po, pd, ow, dw, lit != land.end() ? lit->second : -1.0f,
                   land_haul, p_src, p_d);
            return;
        }
        ++t.un_nonfinite;
        return;
    }

    const float haul = leg.cost; // probe qty 1
    const float net  = p_d - haul;
    if (!(net - p_src > margin * p_src))
    {
        ++t.margin;
        t.margin_haul_sum += haul;
        t.margin_gap_sum += p_d - p_src;
        sample("MARGIN", "p_src %.2f p_d %.2f haul/u %.2f (%s, %d ticks)", p_src, p_d, haul,
               leg.mode == convoy_mode::sea ? "sea" : leg.mode == convoy_mode::space ? "space" : "land",
               leg.travel_ticks);
        return;
    }

    const float landed = p_src + haul;
    const float absorb = dispatch_absorbable(w, reg, d_id, G, landed);
    const float pend   = dispatch_pending(w, reg, d_id, G, corp_sorted, memo);
    const float dshelf = shelf_term ? std::max(0.0f, dm.inventory[G]) : 0.0f;
    const float room   = absorb - pend - dshelf;
    if (!(room > 0.0f))
    {
        ++t.no_room;
        const float hi = dm.base_price[G] * reg.price_band().ceil_mult;
        if (!(absorb > 0.0f))
        {
            if (landed >= hi) ++t.room_landed_over_ceiling;
            else if (dm.demand[G] <= 0.0f) ++t.room_dest_no_demand;
            else ++t.room_absorb_le_pending;
        }
        else
        {
            ++t.room_absorb_le_pending;
            if (dshelf >= absorb) ++t.room_dest_shelf_alone_fills;
        }
        sample("ROOM", "landed %.2f (ceil %.2f) absorbable %.1f pending %.1f dest shelf %.1f "
               "d.demand %.1f d.supply %.1f", landed, hi, absorb, pend, dshelf, dm.demand[G], dm.supply[G]);
        return;
    }
    ++t.go;
    t.go_qty += std::min(surplus, room);
    sample("GO", "net %.2f p_src %.2f haul %.2f room %.1f", net, p_src, haul, room);
}

void refusal_breakdown(world& w, const recipe_registry& reg, std::size_t G, entity_id home,
                       const std::vector<entity_id>& home_markets, const char* when, bool detail)
{
    std::printf("  [BL-1186] refusal breakdown for %s, %s (the decision point of the next "
                "dispatch_convoys)\n", rn(G).c_str(), when);
    const logistics_nodes nodes = collect_logistics_nodes(w);
    std::vector<entity_id> corp_sorted;
    for (const auto& [cid, cc] : w.corporations)
        corp_sorted.push_back(cid);
    std::sort(corp_sorted.begin(), corp_sorted.end());
    std::vector<entity_id> all_markets;
    for (const auto& [mid, m] : w.markets)
        all_markets.push_back(mid);
    std::sort(all_markets.begin(), all_markets.end());
    reservation_memo memo;
    land_cache lands;
    const int max_samples = detail ? 6 : 0;

    // ---- the market's own shelf (export_market_shelves) --------------------
    {
        refusal_tally t;
        long srcs = 0;
        double sur = 0.0;
        int samples = 0;
        int samples_by_kind = 0;
        for (const entity_id s_id : home_markets)
        {
            const market_component& sm = w.markets.at(s_id);
            const float surplus = market_shelf_surplus(w, s_id, G);
            if (!(surplus > 0.0f))
                continue;
            ++srcs;
            sur += surplus;
            const float p_src = dispatch_market_price(sm, G);
            if (detail)
                std::printf("    shelf src m%u: shelf %.1f demand %.1f listed %.1f surplus %.1f "
                            "price %.2f base %.2f (x%.2f) centre water %d port %d\n",
                            static_cast<unsigned>(s_id), sm.inventory[G], sm.demand[G], sm.supply[G],
                            surplus, p_src, sm.base_price[G],
                            sm.base_price[G] > 0 ? p_src / sm.base_price[G] : 0.0f,
                            tile_is_water(w, sm.centre_tile),
                            probe_tile_has_active_port(w, sm.centre_tile));
            for (const entity_id d_id : home_markets)
            {
                if (d_id == s_id)
                    continue;
                classify_pair(w, reg, nodes, corp_sorted, memo, lands, G, null_entity, s_id, home,
                              sm.centre_tile, p_src, surplus, d_id, /*shelf_term=*/true, t,
                              &samples, max_samples, "shelf");
            }
        }
        (void)samples_by_kind;
        print_tally("SHELF", rn(G).c_str(), t, srcs, sur);
    }

    // ---- every corporation's pooled surplus (dispatch_convoys) -------------
    {
        refusal_tally t;
        long srcs = 0, pools_seen = 0, pools_reserved_all = 0, pools_arrived = 0, ordered = 0;
        double sur = 0.0, held = 0.0;
        int samples = 0;
        std::set<std::tuple<entity_id, entity_id, std::size_t>> order_controlled;
        for (const sell_order& o : w.sell_orders)
            if (o.quantity > 0.0f)
                order_controlled.insert({o.corp, o.body, static_cast<std::size_t>(o.resource)});
        for (const entity_id corp : corp_sorted)
        {
            std::vector<entity_id> keys;
            for (auto it = w.corp_market_pools.lower_bound({corp, entity_id{0}});
                 it != w.corp_market_pools.end() && it->first.first == corp; ++it)
                keys.push_back(it->first.second);
            for (const entity_id key : keys)
            {
                const entity_id body = pool_key_body(w, key);
                if (body != home)
                    continue;
                const float q = w.pool_at(corp, key).quantities[G];
                if (!(q > 0.0f))
                    continue;
                ++pools_seen;
                held += q;
                if (order_controlled.count({corp, body, G})) { ++ordered; continue; }
                auto rit = memo.find({corp, key});
                if (rit == memo.end())
                    rit = memo.emplace(std::make_pair(corp, key),
                                       processor_reservation(w, reg, corp, key)).first;
                const float arrived = dispatch_arrived(w, corp, key, G);
                const float surplus = q - rit->second[G] - arrived;
                if (!(surplus > 0.0f))
                {
                    if (arrived > 0.0f) ++pools_arrived; else ++pools_reserved_all;
                    continue;
                }
                ++srcs;
                sur += surplus;
                const float p_src = dispatch_home_price(w, key, G);
                const entity_id origin = convoy_origin_tile(w, w.corporations.at(corp), key);
                for (const entity_id d_id : all_markets)
                {
                    if (d_id == key)
                        continue;
                    classify_pair(w, reg, nodes, corp_sorted, memo, lands, G, corp, key, body,
                                  origin, p_src, surplus, d_id, /*shelf_term=*/false, t, &samples,
                                  max_samples, "corp");
                }
            }
        }
        std::printf("  [BL-1186] CORP pools of %s on the home body: %ld pools hold %.1f; %ld are "
                    "wholly reserved for their own processors, %ld hold only this tick's "
                    "arrivals, %ld under a sell order\n",
                    rn(G).c_str(), pools_seen, held, pools_reserved_all, pools_arrived, ordered);
        print_tally("CORP", rn(G).c_str(), t, srcs, sur);
    }

    // ---- per market: where the good sits, who wants it, what it prices -----
    // Open construction sites needing G per market, and whether the fair-price
    // ceiling (reservation_mult x base, shelf_admits) lets them see the shelf
    // and bid at all.
    if (detail)
    {
        const float res_mult = reg.price_band().reservation_mult;
        std::map<entity_id, int> sites_needing, procs_needing;
        for (const auto& [bid, b] : w.buildings)
        {
            if (body_of_building(w, b) != home)
                continue;
            const entity_id mid = market_for_tile(w, b.tile);
            if (b.ticks_remaining > 0)
            {
                const auto& row = reg.resource_build_cost_for(b.type, b.target_resource, b.recipe);
                if (row[G] > 0.0f)
                    ++sites_needing[mid];
            }
            else if (b.type == building_type::processing_facility && !b.decommissioned)
            {
                const recipe* rc = reg.get_recipe(b.recipe);
                if (rc != nullptr && rc->inputs[G] > 0.0f)
                    ++procs_needing[mid];
            }
        }
        // Processors that draw G and whether they can BID at all: run_processing
        // returns before registering any want when its full-run batch count is
        // zero (economy_system.cpp, `batches_full <= 0`), and batches_full is
        // base_rate x effective workforce x workforce target x supply scalar.
        {
            int n = 0, no_wf = 0, tgt0 = 0, sf0 = 0, can_bid = 0;
            for (const auto& [bid, b] : w.buildings)
            {
                if (body_of_building(w, b) != home || b.type != building_type::processing_facility ||
                    b.ticks_remaining > 0 || b.decommissioned)
                    continue;
                const recipe* rc = reg.get_recipe(b.recipe);
                if (rc == nullptr || rc->inputs[G] <= 0.0f)
                    continue;
                ++n;
                if (b.workforce_assigned <= 0.0f)         ++no_wf;
                else if (b.workforce_target <= 0)         ++tgt0;
                else if (building_supply_scalar(b) <= 0.0f) ++sf0;
                else                                      ++can_bid;
            }
            std::printf("    processors drawing %s: %d; SILENT (no want registered): no workforce "
                        "assigned %d, workforce target 0 %d, supply factor 0 %d; able to bid %d\n",
                        rn(G).c_str(), n, no_wf, tgt0, sf0, can_bid);
        }
        // Every source's verdict toward each destination that WANTS the good
        // (demand registered at the last clear) — the pairs the gap is about.
        {
            refusal_tally scratch;
            for (const entity_id d_id : home_markets)
            {
                if (!(w.markets.at(d_id).demand[G] > 0.0f))
                    continue;
                for (const entity_id s_id : home_markets)
                {
                    if (s_id == d_id)
                        continue;
                    const market_component& sm = w.markets.at(s_id);
                    const float surplus = market_shelf_surplus(w, s_id, G);
                    if (!(surplus > 0.0f))
                        continue;
                    int one = 0;
                    classify_pair(w, reg, nodes, corp_sorted, memo, lands, G, null_entity, s_id,
                                  home, sm.centre_tile, dispatch_market_price(sm, G), surplus, d_id,
                                  true, scratch, &one, 1, "to-WANTING");
                }
            }
        }
        std::printf("    market table (%s): id shelf listed demand price/base admits(<=%.1fx) "
                    "open-sites-needing procs-needing\n", rn(G).c_str(), res_mult);
        for (const entity_id mid : home_markets)
        {
            const market_component& m = w.markets.at(mid);
            const int s = sites_needing.count(mid) ? sites_needing[mid] : 0;
            const int p = procs_needing.count(mid) ? procs_needing[mid] : 0;
            if (m.inventory[G] <= 0.5f && m.demand[G] <= 0.0f && s == 0 && p == 0 &&
                m.supply[G] <= 0.0f)
                continue;
            std::printf("      m%u shelf %8.1f listed %7.1f demand %7.1f price/base %5.2f admits %d "
                        "sites %3d procs %3d\n",
                        static_cast<unsigned>(mid), m.inventory[G], m.supply[G], m.demand[G],
                        m.base_price[G] > 0 ? dispatch_market_price(m, G) / m.base_price[G] : 0.0f,
                        shelf_admits(m, G, res_mult, true), s, p);
        }
    }

    // ---- the passive-LP cap, as it stands before the pass draws on it -------
    {
        lp_pool_map pools;
        auto& body_pools = lp_pool_for_body(pools, w, home, reg.military().active_lp_per_anchor_tick);
        double total = 0.0;
        for (const auto& [a, v] : body_pools)
            total += v;
        int ports = 0;
        for (const auto& [bid, b] : w.buildings)
            if (b.type == building_type::port && b.ticks_remaining <= 0 && !b.decommissioned &&
                body_of_building(w, b) == home)
                ++ports;
        std::printf("  [BL-1186] passive LP on the home body: %zu anchors x %.1f = %.1f u/tick "
                    "(shared by every good and every shipper; one draw per convoy = its qty); "
                    "active ports on the body %d\n",
                    body_pools.size(), reg.military().active_lp_per_anchor_tick, total, ports);
    }
}

void run_seed(lua_state& lua, const options& o, std::uint32_t seed)
{
    world_params p;
    p.seed = seed;
    if (o.epoch >= 0)
        p.epoch_year = o.epoch;
    if (o.fast)
        p = no_prehistory(p);
    auto start = std::make_unique<app_start_world>();
    build_app_start_world(lua, p, *start);
    if (o.lp_scale != 1.0f)
    {
        // A COUNTERFACTUAL, not the shipped world: the passive-LP cap scaled
        // from the first live tick on (the world was built at the shipped cap).
        military_capability_params mp = start->reg.military();
        mp.active_lp_per_anchor_tick *= o.lp_scale;
        start->reg.set_military(mp);
        std::printf("COUNTERFACTUAL: passive LP per anchor x%.2f (NOT the shipped world)\n",
                    o.lp_scale);
    }
    world& w                   = start->w;
    const recipe_registry& reg = start->reg;
    print_shipped_landscape(start->land);

    const entity_id home = w.home_body;
    const std::size_t G  = static_cast<std::size_t>(o.good);
    std::printf("seed %u epoch %lld: home body %u; following makers of %s; t_idle %.2f t_full %.2f%s\n",
                seed, static_cast<long long>(p.epoch_year), static_cast<unsigned>(home), rn(G).c_str(), reg.t_idle(), reg.t_full(),
                o.fast ? "  [--fast: NO prehistory, not the shipped world]" : "");

    std::vector<entity_id> home_markets;
    for (const auto& [mid, m] : w.markets)
        if (m.body == home)
            home_markets.push_back(mid);
    std::sort(home_markets.begin(), home_markets.end());
    std::printf("home markets: %zu\n", home_markets.size());

    // The goods one level up: every input of every recipe (this era) that makes G.
    std::set<std::size_t> input_goods;
    for (int i = 0; i < reg.recipe_count(building_type::processing_facility); ++i)
    {
        const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
        if (rc.outputs[G] <= 0.0f)
            continue;
        std::printf("  recipe %-24s in:", rc.name.c_str());
        for (std::size_t r = 0; r < resource_count; ++r)
            if (rc.inputs[r] > 0.0f)
            {
                std::printf(" %s %.2f", rn(r).c_str(), rc.inputs[r]);
                input_goods.insert(r);
            }
        std::printf("  out: %s %.2f\n", rn(G).c_str(), rc.outputs[G]);
    }

    // The makers of G at the start (tracked by id so a switch or a demolition shows).
    std::set<entity_id> tracked;
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::processing_facility && body_of_building(w, b) == home &&
            recipe_outputs(reg, b.recipe, G))
            tracked.insert(bid);
    std::printf("makers of %s on home body at start: %zu\n", rn(G).c_str(), tracked.size());

    // Goods for the per-tick market line.
    std::vector<std::size_t> market_goods{G};
    for (const std::size_t r : input_goods)
        market_goods.push_back(r);

    std::map<entity_id, float> first_balance;
    std::set<std::string> cost_shown;

    if (o.breakdown)
    {
        std::printf("\n=== seed %u PRE-SETTLE (world built, before econ step 0) ===\n", seed);
        refusal_breakdown(w, reg, G, home, home_markets, "pre-settle", o.detail_ticks.count(-1) > 0);
        std::fflush(stdout);
    }   // each paused kind's build-cost row, printed once

    for (int step = 0; step < o.ticks; ++step)
    {
        // --- run_app_validation_tick, inline (harness_params.hpp) ---------------
        w.current_econ_tick = step;
        lp_pool_map tick_lp_pools;
        advance_convoys(w);
        credit_arrived_convoys(w, 0);
        economy_report report = run_economy_step(w, reg, /*spectating=*/true, &tick_lp_pools);
        if (o.breakdown)
        {
            std::printf("\n--- seed %u tick %d, pre-dispatch ---\n", seed, step);
            const std::string when = "tick " + std::to_string(step) + " pre-dispatch";
            refusal_breakdown(w, reg, G, home, home_markets, when.c_str(),
                              o.detail_ticks.count(step) > 0);
        }
        const convoy_dispatch_tick dt = dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                         reg.logistics_cost(convoy_mode::space), &tick_lp_pools);
        const auto flows = clear_markets(w, reg, report);
        apply_budget(w, reg, flows, report.workforce_contention, &report.budgets,
                     &report.buildings, &report.building_labour);
        run_nation_step(w, reg, report, w.current_econ_tick);
        advance_tech_gates(w);
        (void)compute_corp_standings(w, flows);
        run_firm_exits(w, reg.firm_exit(), &report.firm_exits);
        // ------------------------------------------------------------------------

        const auto own = owners(w);
        auto rep_of = [&](entity_id bid) -> const building_report* {
            const auto it = report.building_row.find(bid);
            return it == report.building_row.end() ? nullptr : &report.buildings[it->second];
        };

        std::printf("\n=== seed %u tick %d ===\n", seed, step);

        // Market line: per good, summed over home markets.
        for (const std::size_t r : market_goods)
        {
            float sup = 0, dem = 0, inv = 0, pr = 0, base = 0;
            int npr = 0;
            for (const entity_id mid : home_markets)
            {
                const market_component& m = w.markets.at(mid);
                sup += m.supply[r];
                dem += m.demand[r];
                inv += m.inventory[r];
                if (m.base_price[r] > 0.0f)
                {
                    pr += m.price[r];
                    base += m.base_price[r];
                    ++npr;
                }
            }
            // Pools: every corp's holding of r on the home body.
            float pool = 0;
            for (const auto& [key, sp] : w.corp_market_pools)
                if (pool_key_body(w, key.second) == home)
                    pool += sp.quantities[r];
            std::printf("  mkt %-16s supply %8.1f demand %8.1f shelf %8.1f pools %8.1f "
                        "price/base %.2f\n",
                        rn(r).c_str(), sup, dem, inv, pool, npr ? pr / base : 0.0f);
        }

        // Per-good state tallies for processors and extractors on the body.
        std::map<std::size_t, state_tally> proc_by_good, ext_by_good;
        state_tally proc_all, ext_all;
        for (const auto& [bid, b] : w.buildings)
        {
            if (body_of_building(w, b) != home)
                continue;
            const building_report* rep = rep_of(bid);
            if (b.type == building_type::processing_facility)
            {
                const char* s = proc_state(b, rep, reg);
                const float q = rep ? rep->output_quantity : 0.0f;
                proc_all.add(s, q);
                if (const recipe* rc = reg.get_recipe(b.recipe))
                    for (std::size_t r = 0; r < resource_count; ++r)
                        if (rc->outputs[r] > 0.0f)
                            proc_by_good[r].add(s, rep && rep->active ? rc->outputs[r] *
                                                     (rep->output_quantity > 0 ? 1.0f : 0.0f) : 0.0f);
            }
            else if (b.type == building_type::extraction_site)
            {
                const char* s = b.ticks_remaining > 0 ? "BUILD"
                              : b.decommissioned      ? "DECOM"
                              : rep == nullptr        ? "NOREP"
                              : rep->active           ? "RUN"
                              : rep->exhausted        ? "EXHST"
                              : rep->effective_workforce <= 0.0f ? "NOLAB" : "IDLE?";
                const float q = rep ? rep->output_quantity : 0.0f;
                ext_all.add(s, q);
                ext_by_good[static_cast<std::size_t>(b.target_resource)].add(s, q);
            }
        }
        auto print_tally = [](const char* label, const state_tally& t) {
            std::printf("  %-26s n %3d run %3d input-starved %3d no-labour %3d decom %3d "
                        "building %3d other %3d  out %8.1f\n",
                        label, t.n, t.run, t.input, t.nolab, t.decom, t.build, t.other, t.out);
        };
        print_tally("ALL processors", proc_all);
        for (const auto& [r, t] : proc_by_good)
            print_tally(("  proc -> " + rn(r)).c_str(), t);
        print_tally("ALL extractors", ext_all);
        for (const auto& [r, t] : ext_by_good)
            if (input_goods.count(r) || r == G)
                print_tally(("  ext  -> " + rn(r)).c_str(), t);

        // Makers of G: tracked + any new ones.
        for (const auto& [bid, b] : w.buildings)
            if (b.type == building_type::processing_facility && body_of_building(w, b) == home &&
                recipe_outputs(reg, b.recipe, G))
                tracked.insert(bid);
        if (!o.quiet)
        {
            std::printf("  makers of %s: bld owner(bal) mkt recipe state wf/eff tgt sf loss "
                        "out | inputs pool+shelf/need | net(rev-in-mt-wg)\n", rn(G).c_str());
            for (const entity_id bid : tracked)
            {
                const auto bit = w.buildings.find(bid);
                const auto oit = own.find(bid);
                if (bit == w.buildings.end())
                {
                    std::printf("    %6u GONE (demolished / wound up)\n", static_cast<unsigned>(bid));
                    continue;
                }
                const building_component& b = bit->second;
                const entity_id corp        = oit == own.end() ? null_entity : oit->second;
                const float bal = corp != null_entity ? w.corporations.at(corp).balance : 0.0f;
                if (corp != null_entity && !first_balance.count(corp))
                    first_balance[corp] = bal;
                const entity_id mid = market_for_tile(w, b.tile);
                const building_report* rep = rep_of(bid);
                const recipe* rc = reg.get_recipe(b.recipe);
                std::printf("    %6u c%-5u(%9.0f) m%-5u %-22s %-5s %.2f/%.2f %3d %4d %2d %6.2f build_left %d+%.2f |",
                            static_cast<unsigned>(bid), static_cast<unsigned>(corp), bal,
                            static_cast<unsigned>(mid), rc ? rc->name.c_str() : "(none)",
                            proc_state(b, rep, reg), b.workforce_assigned,
                            rep ? rep->effective_workforce : 0.0f, b.workforce_target,
                            b.supply_factor_permille, b.loss_streak,
                            rep ? rep->output_quantity : 0.0f, b.ticks_remaining, b.construction_progress);
                if (rc != nullptr)
                {
                    const entity_id key = mid != null_entity ? mid : home;
                    const stockpile_component* pl = w.find_pool(corp, key);
                    const float batches = reg.economics(building_type::processing_facility).base_rate *
                                          (rep ? rep->effective_workforce : 0.0f) *
                                          std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f);
                    for (std::size_t r = 0; r < resource_count; ++r)
                    {
                        if (rc->inputs[r] <= 0.0f)
                            continue;
                        const float pq = pl ? pl->quantities[r] : 0.0f;
                        const float sh = mid != null_entity ? w.markets.at(mid).inventory[r] : 0.0f;
                        const stockpile_component tot = body_pool_total(w, corp, home);
                        std::printf(" %s %.1f+%.1f/%.1f(body %.1f)", rn(r).c_str(), pq, sh,
                                    rc->inputs[r] * batches, tot.quantities[r]);
                    }
                }
                const building_profit bp = estimate_building_profit(w, reg, report, bid);
                if (bp.has_data)
                    std::printf(" | %.1f(%.1f-%.1f-%.1f-%.1f)", bp.net(), bp.revenue, bp.input_cost,
                                bp.maintenance, bp.wages);
                std::printf("\n");
            }
        }

        // Where the inputs are produced, by market, vs where the makers sit.
        {
            std::map<entity_id, int> maker_markets;
            for (const entity_id bid : tracked)
                if (const auto bit = w.buildings.find(bid); bit != w.buildings.end())
                    ++maker_markets[market_for_tile(w, bit->second.tile)];
            std::printf("  makers by market:");
            for (const auto& [m, n] : maker_markets)
                std::printf(" m%u:%d", static_cast<unsigned>(m), n);
            std::printf("\n");
            for (const std::size_t r : input_goods)
            {
                std::map<entity_id, float> out_by_market;
                std::map<entity_id, int>   n_by_market;
                for (const auto& [bid, b] : w.buildings)
                {
                    if (body_of_building(w, b) != home || b.ticks_remaining > 0)
                        continue;
                    bool makes = (b.type == building_type::extraction_site &&
                                  static_cast<std::size_t>(b.target_resource) == r) ||
                                 (b.type == building_type::processing_facility &&
                                  recipe_outputs(reg, b.recipe, r));
                    if (!makes)
                        continue;
                    const entity_id m = market_for_tile(w, b.tile);
                    ++n_by_market[m];
                    if (const building_report* rep = rep_of(bid))
                        out_by_market[m] += rep->output_quantity;
                }
                std::printf("  producers of %-14s by market (n/out):", rn(r).c_str());
                for (const auto& [m, n] : n_by_market)
                    std::printf(" m%u:%d/%.1f%s", static_cast<unsigned>(m), n, out_by_market[m],
                                maker_markets.count(m) ? "*" : "");
                std::printf("\n");
            }
        }

        // Construction on the body: every open site, the material that limits it
        // (run_construction's own rate rule, read from its tile market's SHELF —
        // a site never draws a pool), and whether that limit pauses it.
        {
            const float max_stretch = reg.construction().max_stretch;
            const float pause_below = max_stretch > 1.0f ? 1.0f / max_stretch : 0.0f;
            const std::size_t cap = static_cast<std::size_t>(resource_type::construction_capacity);
            std::map<std::size_t, int> paused_by, slowed_by, paused_empty_shelf;
            std::map<std::string, int> paused_kind;
            int open = 0, full = 0;
            std::set<entity_id> site_markets;
            for (const auto& [bid, b] : w.buildings)
            {
                if (b.ticks_remaining <= 0 || body_of_building(w, b) != home)
                    continue;
                ++open;
                const float dur = reg.economics(b.type).build_duration_ticks;
                if (dur <= 0.0f)
                    continue;
                const auto& row = reg.resource_build_cost_for(b.type, b.target_resource, b.recipe);
                const entity_id mid = market_for_tile(w, b.tile);
                site_markets.insert(mid);
                const market_component* m = mid != null_entity ? &w.markets.at(mid) : nullptr;
                float rate = 1.0f;
                std::size_t lim = resource_count;
                for (std::size_t r = 0; r < resource_count; ++r)
                {
                    if (r == cap || row[r] <= 0.0f)
                        continue;
                    const float cov = (m ? std::max(0.0f, m->inventory[r]) : 0.0f) / (row[r] / dur);
                    if (cov < rate) { rate = cov; lim = r; }
                }
                if (lim == resource_count)          ++full;
                else if (rate < pause_below)
                {
                    ++paused_by[lim];
                    const float shelf = m ? std::max(0.0f, m->inventory[lim]) : 0.0f;
                    if (shelf <= 0.5f) ++paused_empty_shelf[lim];
                    std::string label;
                    if (b.type == building_type::processing_facility)
                    {
                        const recipe* rc = reg.get_recipe(b.recipe);
                        label = rc ? rc->name : "proc?";
                    }
                    else if (b.type == building_type::extraction_site)
                        label = "ext:" + rn(static_cast<std::size_t>(b.target_resource));
                    else
                        label = "type" + std::to_string(static_cast<int>(b.type));
                    ++paused_kind[label];
                    if (!cost_shown.count(label))
                    {
                        cost_shown.insert(label);
                        std::printf("  build cost %-22s (dur %.0f):", label.c_str(), dur);
                        for (std::size_t r = 0; r < resource_count; ++r)
                            if (row[r] > 0.0f)
                                std::printf(" %s %.1f", rn(r).c_str(), row[r]);
                        std::printf("\n");
                    }
                }
                else                                ++slowed_by[lim];
            }
            std::printf("  construction: %d open sites in %zu markets; unblocked-by-materials %d; "
                        "PAUSED by:", open, site_markets.size(), full);
            for (const auto& [r, n] : paused_by)
                std::printf(" %s %d", rn(r).c_str(), n);
            std::printf("; slowed by:");
            for (const auto& [r, n] : slowed_by)
                std::printf(" %s %d", rn(r).c_str(), n);
            std::printf("\n  paused sites whose OWN market shelf of the limiter is empty:");
            for (const auto& [r, n] : paused_empty_shelf)
                std::printf(" %s %d", rn(r).c_str(), n);
            std::printf("\n  paused sites by kind:");
            for (const auto& [k, n] : paused_kind)
                std::printf(" %s %d", k.c_str(), n);
            std::printf("\n");

            // Where the followed good's SHELF sits vs where it is WANTED.
            int with_shelf = 0, with_demand = 0, both = 0;
            float shelf_where_wanted = 0.0f, shelf_where_not = 0.0f, top_shelf = 0.0f;
            entity_id top_m = null_entity;
            for (const entity_id mid : home_markets)
            {
                const market_component& m = w.markets.at(mid);
                const bool s = m.inventory[G] > 0.5f, d = m.demand[G] > 0.0f;
                with_shelf += s;
                with_demand += d;
                both += (s && d);
                (d ? shelf_where_wanted : shelf_where_not) += m.inventory[G];
                if (m.inventory[G] > top_shelf) { top_shelf = m.inventory[G]; top_m = mid; }
            }
            std::printf("  %s geography: %d markets hold shelf, %d markets want it, %d both; shelf "
                        "in wanting markets %.1f, elsewhere %.1f; largest shelf m%u %.1f\n",
                        rn(G).c_str(), with_shelf, with_demand, both, shelf_where_wanted,
                        shelf_where_not, static_cast<unsigned>(top_m), top_shelf);

            // BL-1071: the followed good leaving shelves by the MARKET's own
            // export (owner null) — sent this tick (progress 0: dispatch runs
            // after the tick's advance) and in flight in all.
            int   mx_new = 0;
            float mx_new_q = 0.0f, mx_flight_q = 0.0f, mx_to_wanting = 0.0f;
            for (const convoy_component& cv : w.convoys)
            {
                if (cv.corp != null_entity || static_cast<std::size_t>(cv.cargo_resource) != G)
                    continue;
                mx_flight_q += cv.cargo_qty;
                if (cv.progress == 0.0f)
                {
                    ++mx_new;
                    mx_new_q += cv.cargo_qty;
                    const auto dit = w.markets.find(cv.dest_market);
                    if (dit != w.markets.end() && dit->second.demand[G] > 0.0f)
                        mx_to_wanting += cv.cargo_qty;
                }
            }
            std::printf("  %s market exports: %d sent this tick (%.1f u, %.1f u of it to wanting "
                        "markets); %.1f u in flight | every good: market exports %d, corp "
                        "dispatches %d, refused for passive LP %d\n",
                        rn(G).c_str(), mx_new, mx_new_q, mx_to_wanting, mx_flight_q,
                        dt.market_exports, dt.dispatched, dt.refused_no_lp);

            // WHY a shelf does or does not leave, read after this tick's clear
            // (the prices and demand the NEXT dispatch will act on): every
            // (shelf market, same-body destination) pair, by the first rule of
            // the net-price test it fails. Read-only (price_market_export_leg
            // only warms the A* cache).
            {
                const logistics_nodes nodes = collect_logistics_nodes(w);
                std::vector<entity_id> corp_sorted;
                for (const auto& [cid, cc] : w.corporations)
                    corp_sorted.push_back(cid);
                std::sort(corp_sorted.begin(), corp_sorted.end());
                reservation_memo memo;
                const float margin = reg.dispatch_margin();
                int   srcs = 0;
                long  gate_fail = 0, unroutable = 0, under_margin = 0, no_room = 0, go = 0;
                float shelf_all = 0.0f, surplus_all = 0.0f, demand_at_shelves = 0.0f;
                for (const entity_id s_id : home_markets)
                {
                    const market_component& sm = w.markets.at(s_id);
                    shelf_all += sm.inventory[G];
                    if (sm.inventory[G] > 0.5f)
                        demand_at_shelves += sm.demand[G];
                    const float surplus = market_shelf_surplus(w, s_id, G);
                    if (!(surplus > 0.0f))
                        continue;
                    ++srcs;
                    surplus_all += surplus;
                    const float p_src = dispatch_market_price(sm, G);
                    const float gate  = p_src + margin * p_src;
                    for (const entity_id d_id : home_markets)
                    {
                        if (d_id == s_id)
                            continue;
                        const market_component& dm = w.markets.at(d_id);
                        const float p_d = dispatch_market_price(dm, G);
                        if (!(p_d > gate)) { ++gate_fail; continue; }
                        const convoy_leg leg = price_market_export_leg(w, reg, nodes, s_id, d_id, 1.0f);
                        if (!leg.viable) { ++unroutable; continue; }
                        const float net = p_d - leg.cost;
                        if (!(net - p_src > margin * p_src)) { ++under_margin; continue; }
                        const float room = dispatch_absorbable(w, reg, d_id, G, p_src + leg.cost)
                                         - dispatch_pending(w, reg, d_id, G, corp_sorted, memo)
                                         - std::max(0.0f, dm.inventory[G]);
                        if (!(room > 0.0f)) { ++no_room; continue; }
                        ++go;
                    }
                }
                std::printf("  %s shelf test (next dispatch): shelf %.1f, local demand at shelf "
                            "markets %.1f, surplus %.1f on %d markets | pairs: price<=gate %ld, "
                            "unroutable %ld, net under margin %ld, no room %ld, would send %ld\n",
                            rn(G).c_str(), shelf_all, demand_at_shelves, surplus_all, srcs,
                            gate_fail, unroutable, under_margin, no_room, go);
            }
        }

        // Events touching the body.
        for (const agency_event& ev : report.agency_events)
        {
            const auto bit = w.buildings.find(ev.building);
            const bool on_home = bit != w.buildings.end() && body_of_building(w, bit->second) == home;
            if (!on_home)
                continue;
            const bool tracked_b = tracked.count(ev.building) > 0;
            const char* k = "?";
            switch (ev.what)
            {
            case agency_event::kind::recipe_switch: k = "recipe_switch"; break;
            case agency_event::kind::idled:         k = "idled"; break;
            case agency_event::kind::built:         k = "built"; break;
            case agency_event::kind::demolished:    k = "demolished"; break;
            case agency_event::kind::workforce_set: k = "workforce_set"; break;
            case agency_event::kind::resumed:       k = "resumed"; break;
            default: continue;
            }
            if (!tracked_b && ev.what == agency_event::kind::workforce_set)
                continue;
            const recipe* nr = ev.what == agency_event::kind::recipe_switch
                                   ? reg.get_recipe(ev.new_recipe) : nullptr;
            std::printf("  event %s bld %u corp %u%s%s value %d\n", k,
                        static_cast<unsigned>(ev.building), static_cast<unsigned>(ev.corp),
                        tracked_b ? " [MAKER]" : "", nr ? (" -> " + nr->name).c_str() : "",
                        ev.value);
        }
        for (const firm_exit_record& fx : report.firm_exits)
            std::printf("  firm_exit corp %u bal %.0f holdings %d\n",
                        static_cast<unsigned>(fx.corp), fx.balance, fx.holdings);
        std::fflush(stdout);
    }
}

} // namespace

int main(int argc, char** argv)
{
    options o;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--seeds" && i + 1 < argc)
        {
            o.seeds.clear();
            std::string s = argv[++i];
            std::size_t pos = 0;
            while (pos <= s.size())
            {
                const std::size_t c = s.find(',', pos);
                o.seeds.push_back(static_cast<std::uint32_t>(
                    std::strtoul(s.substr(pos, c - pos).c_str(), nullptr, 10)));
                if (c == std::string::npos)
                    break;
                pos = c + 1;
            }
        }
        else if (a == "--ticks" && i + 1 < argc) o.ticks = std::atoi(argv[++i]);
        else if (a == "--fast")                  o.fast = true;
        else if (a == "--quiet")                 o.quiet = true;
        else if (a == "--no-breakdown")          o.breakdown = false;
        else if (a == "--detail-ticks" && i + 1 < argc)
        {
            o.detail_ticks.clear();
            std::string s = argv[++i];
            std::size_t pos = 0;
            while (pos <= s.size())
            {
                const std::size_t c = s.find(',', pos);
                o.detail_ticks.insert(std::atoi(s.substr(pos, c - pos).c_str()));
                if (c == std::string::npos)
                    break;
                pos = c + 1;
            }
        }
        else if (a == "--epoch" && i + 1 < argc) o.epoch = std::atoll(argv[++i]);
        else if (a == "--lp-scale" && i + 1 < argc) o.lp_scale = static_cast<float>(std::atof(argv[++i]));
        else if (a == "--good" && i + 1 < argc)
        {
            bool ok = false;
            o.good = resource_names::resource_from_name(argv[++i], ok);
            if (!ok)
            {
                std::printf("unknown good\n");
                return 2;
            }
        }
        else
        {
            std::printf("usage: %s [--seeds 0,1,2] [--ticks N] [--fast] [--quiet] [--good g]\n",
                        argv[0]);
            return 2;
        }
    }
    lua_state lua;
    for (const std::uint32_t s : o.seeds)
        run_seed(lua, o, s);
    return 0;
}
