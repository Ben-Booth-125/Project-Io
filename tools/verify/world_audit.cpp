// Throwaway headless harness: builds the hard-coded world and reports
//   (S2) Kepler's forest + wetland tile fraction (biome-balance target ~>=3%);
//   (S1) whether every placed extraction asset sits on a tile carrying a
//        non-zero prototype-extractable deposit (iron ore / petroleum / water /
//        agricultural produce) on valid (non-ocean) terrain.
// No SDL / Lua / ImGui. Kept outside src/ so the CMake glob ignores it.

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "world/placement_rules.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <utility>
#include <vector>

static std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

int main()
{
    world w = make_hard_coded_world();

    // --- S2: Kepler composition histogram ---
    const entity_id kepler = w.home_body;
    std::map<int, int> hist;
    int land = 0, total = 0;
    int forest = 0, wetland = 0;
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != kepler)
            continue;
        ++total;
        ++hist[static_cast<int>(tc.composition)];
        if (tc.composition != terrain_composition::ocean)
            ++land;
        if (tc.composition == terrain_composition::forest)  ++forest;
        if (tc.composition == terrain_composition::wetland) ++wetland;
    }

    std::printf("Kepler tiles: %d total, %d land\n", total, land);
    const float fw_frac = total ? 100.0f * static_cast<float>(forest + wetland) / total : 0.0f;
    std::printf("  forest=%d (%.2f%%)  wetland=%d (%.2f%%)  forest+wetland=%.2f%% of all tiles\n",
                forest, total ? 100.0f * forest / total : 0.0f,
                wetland, total ? 100.0f * wetland / total : 0.0f, fw_frac);
    std::printf("  S2 target forest+wetland >= 3%% of all tiles: %s\n",
                fw_frac >= 3.0f ? "PASS" : "FAIL");

    // --- S1: extraction asset placement audit ---
    const resource_type extractable[] = {
        resource_type::iron_ore, resource_type::petroleum,
        resource_type::water, resource_type::agricultural_produce };

    int extraction_assets = 0, bad = 0;
    for (const auto& [cid, corp] : w.corporations)
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
            bool ocean = (tc.composition == terrain_composition::ocean);
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
    for (const auto& [cid, corp] : w.corporations)
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
    ocean_tile.composition = terrain_composition::ocean;
    ocean_tile.resource_deposit[ri(resource_type::iron_ore)] = 10.0f; // deposit present, but ocean
    const bool ocean_rejected = !placement_rules::can_place(
        ocean_tile, building_type::extraction_site, resource_type::iron_ore);

    tile_component dry_tile{};
    dry_tile.composition = terrain_composition::barren; // land, but no iron deposit
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
        if (tc.body != kepler || tc.composition == terrain_composition::ocean)
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
    // extraction 3..4, processing 2..3, trade 1..2. ---
    auto focus_bounds = [](industrial_focus f) -> std::pair<int,int> {
        switch (f)
        {
            case industrial_focus::extraction: return { 3, 4 };
            case industrial_focus::processing: return { 2, 3 };
            case industrial_focus::trade:      return { 1, 2 };
        }
        return { 1, 2 };
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
    for (const auto& [cid, corp] : w.corporations)
    {
        const int count = static_cast<int>(corp.assets.size());
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
    // fixed give). make_hard_coded_world() is the cold generation state (no
    // pre-game ticks), so pools equal the generated give exactly. Checks:
    //   R1 every corp with holdings opens with a non-empty stockpile on its home
    //      body, scoped to the seven prototype resources (nothing else stocked);
    //   R2 focus correlation — extraction corps open richer in raws than trade;
    //   R3 determinism — a second generation yields identical stockpiles. ---
    auto home_body_of = [&](const corporation_component& corp) -> entity_id {
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
    for (const auto& [cid, corp] : w.corporations)
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
        const auto pit = w.corp_body_pools.find(std::make_pair(cid, hb));
        if (pit == w.corp_body_pools.end())
        {
            ++stock_bad;
            std::printf("  BAD: corp=%u has no stockpile on its home body\n",
                        static_cast<unsigned>(cid));
            continue;
        }
        const auto& q = pit->second.quantities;
        float total = 0.0f;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            total += q[r];
            if (!is_prototype(r) && q[r] != 0.0f)
            {
                ++stock_bad;
                std::printf("  BAD: corp=%u non-prototype res %zu stocked (%.2f)\n",
                            static_cast<unsigned>(cid), r, q[r]);
            }
        }
        if (total <= 0.0f)
        {
            ++stock_bad;
            std::printf("  BAD: corp=%u opens with an empty stockpile\n",
                        static_cast<unsigned>(cid));
        }
        float raw = 0.0f;
        for (resource_type rr : raw_set) raw += q[ri(rr)];
        if (corp.focus == industrial_focus::extraction) { ext_raw_sum += raw; ++ext_n; }
        else if (corp.focus == industrial_focus::trade)  { trade_raw_sum += raw; ++trade_n; }
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

    world w2 = make_hard_coded_world();
    bool det_ok = (w.corp_body_pools.size() == w2.corp_body_pools.size());
    int det_bad = 0;
    for (const auto& [key, pool] : w.corp_body_pools)
    {
        const auto it2 = w2.corp_body_pools.find(key);
        if (it2 == w2.corp_body_pools.end()) { ++det_bad; continue; }
        for (std::size_t r = 0; r < resource_count; ++r)
            if (pool.quantities[r] != it2->second.quantities[r]) { ++det_bad; break; }
    }
    if (det_bad != 0) det_ok = false;
    std::printf("  BL-116 R3 stockpiles identical across two generations (%zu pools, %d mismatched): %s\n",
                w.corp_body_pools.size(), det_bad, det_ok ? "PASS" : "FAIL");

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

    // --- BL-053: country count + size-variance audit ---
    // Only Kepler generates nations, so w.nations is the Kepler political layer.
    const int nation_n = static_cast<int>(w.nations.size());
    int min_tiles = -1, max_tiles = 0;
    for (const auto& [nid, nat] : w.nations)
    {
        const int t = static_cast<int>(nat.tiles.size());
        if (min_tiles < 0 || t < min_tiles) min_tiles = t;
        if (t > max_tiles) max_tiles = t;
    }
    if (min_tiles < 0) min_tiles = 0;
    const bool count_ok    = nation_n >= 20 && nation_n <= 28;
    const bool variance_ok = min_tiles > 0 && max_tiles >= 3 * min_tiles;
    std::printf("Nations: %d (min tiles %d, max tiles %d)\n", nation_n, min_tiles, max_tiles);
    std::printf("  BL-053 R1 nation count in [20,28]: %s\n", count_ok ? "PASS" : "FAIL");
    std::printf("  BL-053 R2 strong size variance (max >= 3x min): %s\n",
                variance_ok ? "PASS" : "FAIL");

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

    world wm2 = make_hard_coded_world();
    std::vector<entity_id> centre_tiles2;
    for (const auto& [mid, mc] : wm2.markets)
        if (mc.body == wm2.home_body)
            centre_tiles2.push_back(mc.centre_tile);
    std::sort(centre_tiles2.begin(), centre_tiles2.end());
    const bool market_determinism_ok = (centre_tiles == centre_tiles2);
    std::printf("  BL-096 R4 same seed reproduces the market map: %s\n",
                market_determinism_ok ? "PASS" : "FAIL");

    return (fw_frac >= 3.0f && bad == 0 && seed_bad == 0 && seam_bad == 0
            && unclaimed_land == 0 && holdings_bad == 0 && stockpile_ok
            && absent == 0 && ordering_ok
            && count_ok && variance_ok
            && market_count_ok && cross_nation_ok && market_determinism_ok) ? 0 : 1;
}
