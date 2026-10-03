// ---------------------------------------------------------------------------
// The world digests world_determinism holds generation to (BL-1084: shared)
// ---------------------------------------------------------------------------
// MOVED VERBATIM out of tools/verify/world_determinism.cpp so a second harness
// can hold a world to the SAME digest rather than a second one written beside
// it (tools/verify/world_cursor_equivalence.cpp, BL-1084). Nothing in the
// folds changed in the move; world_determinism prints the same digests.
//
// Single-TU harness header: the definitions sit in the unnamed namespace, as
// they did in world_determinism.cpp, so each harness keeps its own copy.
#pragma once

#include "world/components.hpp"
#include "world/era_minus_one.hpp"   // the fixture: the navy and the corridor set
#include "world/history_sim.hpp"     // polity::navy_stock, history_corridor
#include "world/settlement.hpp"      // region stocks on world::gen_settlement
#include "world/stockpile_budget.hpp" // BL-1042: the stockpile fold
#include "world/world.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace {

struct world_metrics
{
    std::size_t        tiles    = 0;
    std::size_t        nations  = 0;
    std::size_t        corps    = 0;
    std::size_t        entities = 0; ///< sum of the public component containers
    std::map<int, int> comp_hist;    ///< tiles per (substrate, cover) pair (BL-519)
    double             deposit_total = 0.0;

    bool operator==(const world_metrics& o) const
    {
        return tiles == o.tiles && nations == o.nations && corps == o.corps &&
               entities == o.entities && comp_hist == o.comp_hist &&
               deposit_total == o.deposit_total;
    }
};

world_metrics measure(const world& w)
{
    world_metrics m;
    m.tiles    = w.tiles.size();
    m.nations  = w.nations.size();
    m.corps    = w.corporations.size();
    m.entities = w.bodies.size() + w.tiles.size() + w.buildings.size() +
                 w.stockpiles.size() + w.markets.size() + w.units.size() +
                 w.population_centres.size() + w.nations.size() + w.corporations.size();
    for (const auto& [id, tc] : w.tiles)
    {
        // BOTH AXES fold into the digest (BL-519). Hashing only the substrate
        // would let a cover regression pass a determinism check silently, which
        // is exactly the hole a split axis opens if the digest is not widened
        // with it.
        ++m.comp_hist[static_cast<int>(tc.substrate) * 16 + static_cast<int>(tc.cover)];
        m.deposit_total += static_cast<double>(tc.cover_density) * 1e-6;
        for (std::size_t r = 0; r < resource_count; ++r)
            m.deposit_total += tc.resource_deposit[r];
    }
    return m;
}

// ---------------------------------------------------------------------------
// The deep digest (R3 only)
// ---------------------------------------------------------------------------
// `world_metrics` above is a TILE-AND-COUNT digest: terrain histogram, summed
// deposits, container sizes. That is the right instrument for R1/R2, which ask
// whether the seed and the abundance tier drive TILE generation.
//
// It is the wrong instrument for the Era -1 year-tick sim, and reusing it for
// R3 would be the vacuity trap this file exists to catch elsewhere. The sim
// runs AFTER the tile surface is fixed and changes nothing in it — it moves
// regions between owners, and reaches the world only through the nation
// carve, the derived national character, the generated names and the corporate
// charter placement downstream of them. Two worlds with entirely different
// political histories can hold the identical terrain histogram, the identical
// deposit total and the identical container sizes.
//
// So R3 compares a whole-world FNV-1a instead: `world::state_hash` — the
// project's own canonical snapshot primitive (corps, buildings, markets, pools,
// tile reserves, units, order book) — folded together with the political layer
// state_hash omits. state_hash omits it on purpose: it is a TICK-BOUNDARY
// instrument, and nations, borders and city names do not move on a tick. They
// are precisely what the pre-history pass writes, so R3 hashes them itself.
//
// Used ONLY by R3. R1/R2 keep the metrics they were written against, so
// nothing about the existing cases changes.
//
// THE DIGEST PROVES SAME-SEED-SAME-WORLD ONLY FOR THE FIELDS IT FOLDS (BL-1009,
// with BL-957 merged in). Twice a real world-mover slipped through with every
// digest digit-identical: moving world setup from 1200 grudges and corridors to
// 1660 ones re-seeded sentiment by hundreds of rows and re-stamped hundreds of
// road tiles (BL-956), and folding every held seat into the capital moved the
// mean capital treasury by 80% and the Post Road count by 25 (BL-998). Neither
// field was read here. So the digest now folds, beside the political layer:
//
//   - seeded sentiment   `world::sentiment` (every row, both dimensions);
//   - road tiers         `tile_component::road_level`, per tile;
//   - sea lanes          `tile_component::lane_level`, per tile (BL-1098; folded
//                        only where a lane exists);
//   - region stocks      `world::gen_settlement->regions`: `treasury`,
//                        `port_stock_q`, `standing_army` and its owner;
//   - the stockpile      (BL-1042) every region's `industry_points`, and the
//                        charter budget `build_stockpile_budget` makes of them
//                        (per centre, and every unspent reason) — FOLDED ONLY
//                        WHEN SOME REGION HOLDS A POINT, so a span-off world
//                        (no point anywhere) hashes exactly as it did before
//                        the fold existed. Complete as a detector on the same
//                        argument as the nation treasury below: two worlds
//                        differing in any region's points have a non-zero on
//                        at least one side, so at least one folds;
//   - the navy           `polity::navy_stock` off the Exploration handoff;
//   - corridor tiers     the corridor set world setup stamped roads from, per
//                        tier and per corridor.
//
// NATION TREASURY NEEDS NO FOLD OF ITS OWN: `world::state_hash`, which seeds this
// digest, already folds `nation_component::treasury` whenever any is non-zero,
// and that conditional is complete as a detector (two worlds differing in any
// treasury have a non-zero on at least one side).
//
// THE LAST TWO ARE NOT WORLD STATE, which is why the digest takes the fixture.
// A navy lives on the Era -1 polity and never lands on `world`; the corridor
// record is a generation local that `stamp_history_roads` and the junction
// markets consume. Both are what the span hands forward, and a regression in
// either is otherwise invisible until something downstream happens to read it.
// They are read off the SAME fixture generation filled, never re-derived.

constexpr uint64_t fnv_prime = 1099511628211ull;

void fold_bytes(uint64_t& h, const void* p, std::size_t n)
{
    const auto* b = static_cast<const unsigned char*>(p);
    for (std::size_t i = 0; i < n; ++i)
    {
        h ^= static_cast<uint64_t>(b[i]);
        h *= fnv_prime;
    }
}

void fold_u32(uint64_t& h, uint32_t v) { fold_bytes(h, &v, sizeof v); }
void fold_i32(uint64_t& h, int32_t v) { fold_bytes(h, &v, sizeof v); }
void fold_i64(uint64_t& h, int64_t v) { fold_bytes(h, &v, sizeof v); }
void fold_f32(uint64_t& h, float v) { fold_bytes(h, &v, sizeof v); }

void fold_str(uint64_t& h, const std::string& s)
{
    fold_u32(h, static_cast<uint32_t>(s.size()));
    fold_bytes(h, s.data(), s.size());
}

/// Keys of an unordered container, sorted — the same canonicalisation
/// world::state_hash performs, for the same reason (hash order is not state).
template <typename Map>
std::vector<entity_id> sorted_ids(const Map& m)
{
    std::vector<entity_id> ids;
    ids.reserve(m.size());
    for (const auto& kv : m) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

uint64_t deep_digest(const world& w, const era_minus_one_fixture& fx)
{
    // Seed the fold with the canonical snapshot rather than repeating it.
    uint64_t h = w.state_hash(0);

    for (const entity_id id : sorted_ids(w.bodies))
    {
        const body_component& b = w.bodies.at(id);
        fold_u32(h, id);
        fold_str(h, b.name);
        fold_i32(h, b.grid_width);
        fold_i32(h, b.grid_height);
        fold_f32(h, b.mass_earths);
        fold_f32(h, b.orbital_radius_au);
        fold_i32(h, static_cast<int32_t>(b.type));
    }

    // The political carve — what the pre-history pass actually rewrites.
    for (const entity_id id : sorted_ids(w.nations))
    {
        const nation_component& n = w.nations.at(id);
        fold_u32(h, id);
        fold_str(h, n.name);
        fold_u32(h, static_cast<uint32_t>(n.tiles.size()));
        for (const entity_id t : n.tiles) fold_u32(h, t); // ordered: Pass 2 output
        for (const float a : n.resource_abundance) fold_f32(h, a);
        fold_i32(h, static_cast<int32_t>(n.politics));
        fold_i32(h, static_cast<int32_t>(n.posture));
        fold_i32(h, static_cast<int32_t>(n.focus));
    }

    for (const entity_id t : sorted_ids(w.tile_to_nation))
    {
        fold_u32(h, t);
        fold_u32(h, w.tile_to_nation.at(t));
    }

    for (const entity_id id : sorted_ids(w.population_centres))
    {
        const population_centre_component& p = w.population_centres.at(id);
        fold_u32(h, id);
        fold_i32(h, p.scale);
        fold_i32(h, p.population);
        fold_f32(h, p.habitability);
        fold_i32(h, p.growth_accumulator);
        const auto tit = w.population_centre_tile.find(id);
        fold_u32(h, tit != w.population_centre_tile.end() ? tit->second : null_entity);
        const auto nit = w.population_centre_name.find(id);
        if (nit != w.population_centre_name.end()) fold_str(h, nit->second);
    }

    for (const entity_id id : sorted_ids(w.corporations))
    {
        const corporation_component& c = w.corporations.at(id);
        fold_u32(h, id);
        fold_str(h, c.name);
        fold_u32(h, c.home_nation);
        fold_i32(h, static_cast<int32_t>(c.focus));
        fold_f32(h, c.starting_capital);
        fold_i32(h, c.is_player ? 1 : 0);
        fold_u32(h, static_cast<uint32_t>(c.assets.size()));
        for (const entity_id a : c.assets) fold_u32(h, a);
    }

    // The world log: generation narrates into it, so a pass that ran
    // differently shows up here as prose even when nothing numeric moved.
    fold_u32(h, static_cast<uint32_t>(w.history_log.size()));
    for (const world_history_entry& e : w.history_log)
    {
        fold_i64(h, e.timestamp);
        fold_i32(h, static_cast<int32_t>(e.topic));
        fold_u32(h, e.body);
        fold_u32(h, e.corp);
        fold_str(h, e.event);
        fold_str(h, e.consequence);
    }

    // --- BL-1009: what generation moves that play reads ---------------------
    // Every section folds its size first, so an empty table and a table of
    // one zero-valued row cannot collide.

    // Seeded sentiment (BL-898's grudge rows). `sentiment_table::pairs` is a
    // std::map keyed (observer, subject), so this is already a sorted walk.
    fold_u32(h, static_cast<uint32_t>(w.sentiment.pairs.size()));
    for (const auto& [pair, v] : w.sentiment.pairs)
    {
        fold_u32(h, pair.first);
        fold_u32(h, pair.second);
        fold_f32(h, v.access);
        fold_f32(h, v.trust);
    }

    // Road tier per tile, SPARSE: (tile, tier) for every tile carrying a road,
    // in ascending tile id. Complete as a detector — two worlds that differ on
    // any tile's tier differ in this list — and it keeps the walk proportional
    // to the network rather than to the grid.
    {
        std::vector<entity_id> roads;
        for (const auto& [tid, tc] : w.tiles)
            if (tc.road_level != 0) roads.push_back(tid);
        std::sort(roads.begin(), roads.end());
        fold_u32(h, static_cast<uint32_t>(roads.size()));
        for (const entity_id tid : roads)
        {
            fold_u32(h, tid);
            fold_u32(h, w.tiles.at(tid).road_level);
        }
    }

    // BL-1098: sea-lane tier per tile, beside the roads and on their sparse
    // terms: (tile, tier) for every tile carrying a lane, ascending tile id.
    // FOLDED ONLY WHEN SOME TILE CARRIES A LANE (the stockpile's precedent), so
    // a world no span ran hashes exactly as it did before the fold existed;
    // complete as a detector all the same -- two worlds differing on any tile's
    // lane have a lane on at least one side, so at least one folds.
    {
        std::vector<entity_id> lanes;
        for (const auto& [tid, tc] : w.tiles)
            if (tc.lane_level != 0) lanes.push_back(tid);
        if (!lanes.empty())
        {
            std::sort(lanes.begin(), lanes.end());
            fold_u32(h, 0x4C414E45u); // "LANE": the section's tag
            fold_u32(h, static_cast<uint32_t>(lanes.size()));
            for (const entity_id tid : lanes)
            {
                fold_u32(h, tid);
                fold_u32(h, w.tiles.at(tid).lane_level);
            }
        }
    }

    // Region stocks, in region index order (a vector: its order is the
    // settlement's placement order, which is itself generation output). The
    // absent record folds a sentinel so "no settlement" cannot hash as "a
    // settlement of no regions".
    if (const settlement_state* ss = w.gen_settlement.get())
    {
        fold_u32(h, static_cast<uint32_t>(ss->regions.size()));
        for (const region& rg : ss->regions)
        {
            fold_i64(h, rg.treasury);
            fold_i32(h, rg.port_stock_q);
            fold_i64(h, rg.standing_army);
            fold_i32(h, rg.standing_army_owner);
        }
    }
    else
    {
        fold_u32(h, 0xFFFFFFFFu);
    }

    // BL-1042 — THE STOCKPILE, conditional (see the header): nothing is folded
    // while every region holds zero points, which is every span-off world.
    if (const settlement_state* ss = w.gen_settlement.get())
    {
        bool any = false;
        for (const region& rg : ss->regions)
            any = any || rg.industry_points != 0;
        if (any)
        {
            fold_u32(h, 0x10420000u);   // a section tag: "the stockpile follows"
            for (const region& rg : ss->regions)
                fold_i64(h, rg.industry_points);
            const stockpile_budget sb = build_stockpile_budget(w);
            fold_i32(h, sb.rejected ? 1 : 0);
            fold_i64(h, sb.points_total);
            fold_i32(h, sb.firm_price_points);   // BL-1064: the price the stock derives
            for (const std::int64_t u : sb.unspent)
                fold_i64(h, u);
            fold_u32(h, static_cast<uint32_t>(sb.budget.points().size()));
            for (const auto& [centre, pts] : sb.budget.points())   // std::map: ascending id
            {
                fold_u32(h, centre);
                fold_i32(h, pts);
            }
            // BL-1168: the reach prices, folded only off the world reading so a
            // world-priced budget digests as it did before the reach existed.
            if (sb.reach != charter_price_reach::world)
            {
                fold_u32(h, 0x11680000u | static_cast<uint32_t>(sb.reach));
                for (const auto& [centre, fp] : sb.centre_firm_price)   // ascending id
                {
                    fold_u32(h, centre);
                    fold_i32(h, fp);
                }
            }
        }
    }

    // The navy, off the Exploration handoff's polity table (index order).
    // Empty — and folded as a zero size — wherever the span did not run.
    fold_i32(h, fx.exploration_ran ? 1 : 0);
    fold_u32(h, static_cast<uint32_t>(fx.exploration_handoff.polities.size()));
    for (const polity& p : fx.exploration_handoff.polities)
        fold_i64(h, p.navy_stock);

    // The corridor set world setup stamped roads and junction markets from:
    // the per-tier counts, then every corridor's (a, b, tier) in (a, b) order.
    // Sorted here rather than trusted, so the fold cannot lean on either
    // span's own ordering. `uses` is deliberately NOT folded — it is traffic,
    // and nothing past generation reads it; the tier is what reaches the map.
    {
        std::vector<history_corridor> cs = fx.setup_corridors;
        std::sort(cs.begin(), cs.end(),
                  [](const history_corridor& x, const history_corridor& y) {
                      return x.a != y.a ? x.a < y.a : x.b < y.b;
                  });
        std::map<int, uint32_t> tier_count;
        for (const history_corridor& c : cs) ++tier_count[c.tier];
        fold_u32(h, static_cast<uint32_t>(cs.size()));
        fold_u32(h, static_cast<uint32_t>(tier_count.size()));
        for (const auto& [tier, n] : tier_count)
        {
            fold_i32(h, tier);
            fold_u32(h, n);
        }
        for (const history_corridor& c : cs)
        {
            fold_u32(h, c.a);
            fold_u32(h, c.b);
            fold_u32(h, c.tier);
        }
    }

    return h;
}

} // namespace
