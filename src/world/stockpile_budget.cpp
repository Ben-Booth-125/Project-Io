#include "stockpile_budget.hpp"

#include "history_sim.hpp"   // industry_points_ceiling
#include "market_clearing.hpp" // market_for_tile (BL-1168's market reach)
#include "ocean_currents.hpp"  // landmass_labels, landmass_at (BL-1168's landmass reach)
#include "corporation_generation.hpp"  // assign_default_recipes (NR-909's seed-candidate spend)
#include "landscape_search.hpp"        // apply_landscape_candidate (NR-909)
#include "settlement.hpp"
#include "world.hpp"

#include <algorithm>
#include <limits>
#include <map>

static_assert(industry_points_ceiling <= (1LL << 60),
              "stockpile_points_total_max assumes a region holds at most 2^60 points");

namespace {

/// One carved slot of a region: a founded centre (id) or a dropped one (null).
struct slot_ref
{
    int                      rank   = 0;
    std::int64_t             key    = 0;
    entity_id                centre = null_entity;
    stockpile_unspent_reason drop   = stockpile_unspent_reason::carve_dropped;
};

stockpile_budget rejected_budget(std::int64_t points_total, std::vector<stockpile_region_row> rows,
                                 const std::string& why)
{
    stockpile_budget out;
    out.rejected     = true;
    out.rejection    = why;
    out.points_total = points_total;
    out.unspent[static_cast<std::size_t>(stockpile_unspent_reason::rejected)] = points_total;
    for (stockpile_region_row& r : rows)
    {
        r.to_centres = 0;
        r.founded    = 0;
        r.dropped    = 0;
        r.unspent    = {};
        r.unspent[static_cast<std::size_t>(stockpile_unspent_reason::rejected)] = r.points;
    }
    out.regions = std::move(rows);
    return out;
}

} // namespace

std::vector<std::int64_t> stockpile_region_reach(const world& w, charter_price_reach reach)
{
    const settlement_state* ss = w.gen_settlement.get();
    if (ss == nullptr)
        return {};
    return stockpile_region_reach(w, ss->regions, reach);
}

std::vector<std::int64_t> stockpile_region_reach(const world& w, const std::vector<region>& regions,
                                                 charter_price_reach reach)
{
    std::vector<std::int64_t> out;
    if (reach == charter_price_reach::world)
        return out;
    const auto b = w.bodies.find(w.home_body);
    if (b == w.bodies.end())
        return out;
    const int gw = b->second.grid_width, gh = b->second.grid_height;
    if (gw <= 0 || gh <= 0)
        return out;

    // The home body's raster, filled by grid position: no iteration order of
    // the tile map reaches it. A cell no tile fills reads as ocean (sea) and
    // holds no tile.
    const std::size_t cells = static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh);
    std::vector<terrain_substrate> substrate(cells, terrain_substrate::ocean);
    std::vector<entity_id>         tile_at(cells, null_entity);
    for (const auto& [tid, t] : w.tiles)
    {
        if (t.body != w.home_body || t.grid_x < 0 || t.grid_y < 0 || t.grid_x >= gw || t.grid_y >= gh)
            continue;
        const std::size_t i = static_cast<std::size_t>(t.grid_y) * static_cast<std::size_t>(gw)
                            + static_cast<std::size_t>(t.grid_x);
        substrate[i] = t.substrate;
        tile_at[i]   = tid;
    }

    out.assign(regions.size(), -1);
    if (reach == charter_price_reach::landmass)
    {
        const std::vector<int32_t> labels = landmass_labels(substrate, gw, gh);
        for (std::size_t i = 0; i < regions.size(); ++i)
            out[i] = landmass_at(labels, gw, gh, regions[i].col, regions[i].row);
    }
    else
    {
        for (std::size_t i = 0; i < regions.size(); ++i)
        {
            const region& rg = regions[i];
            if (rg.col < 0 || rg.row < 0 || rg.col >= gw || rg.row >= gh)
                continue;
            const entity_id tid = tile_at[static_cast<std::size_t>(rg.row) * static_cast<std::size_t>(gw)
                                          + static_cast<std::size_t>(rg.col)];
            if (tid == null_entity)
                continue;
            const entity_id m = market_for_tile(w, tid);
            if (m != null_entity)
                out[i] = static_cast<std::int64_t>(m);
        }
    }
    return out;
}

stockpile_budget build_stockpile_budget(const world& w, std::int64_t price_divisor,
                                        charter_price_reach reach)
{
    const settlement_state* ss = w.gen_settlement.get();
    // No home-body grid to read a reach off (a hand-built fixture): every
    // centre pays the world's price, and the budget says `world`.
    const std::vector<std::int64_t> region_reach = stockpile_region_reach(w, reach);
    const bool read = reach != charter_price_reach::world && !region_reach.empty();
    return build_stockpile_budget(ss != nullptr ? &ss->regions : nullptr,
                                  w.gen_carve_centres, w.gen_carve_dropped, price_divisor,
                                  read ? &region_reach : nullptr,
                                  read ? reach : charter_price_reach::world);
}

stockpile_budget build_stockpile_budget(const std::vector<region>*             regions,
                                        const std::map<entity_id, carve_slot>& founded,
                                        const std::vector<carve_dropped_slot>& dropped,
                                        std::int64_t                           price_divisor,
                                        const std::vector<std::int64_t>*       region_reach,
                                        charter_price_reach                          reach)
{
    stockpile_budget out;
    if (regions == nullptr)
        return out;   // a loaded world or a fixture: no stockpile, nothing to account
    if (reach == charter_price_reach::world)
        region_reach = nullptr;

    // --- the stock, per region ----------------------------------------------
    std::vector<stockpile_region_row> rows;
    std::int64_t total = 0;
    for (std::size_t ri = 0; ri < regions->size(); ++ri)
    {
        const std::int64_t pts = (*regions)[ri].industry_points;
        if (pts == 0)
            continue;
        stockpile_region_row row;
        row.region = static_cast<int>(ri);
        row.points = pts;
        if (pts < 0 || pts > industry_points_ceiling)
        {
            // Not added to the rows or the total: a value outside the domain
            // is not a number of points (see `stockpile_budget::rejected`).
            return rejected_budget(total, std::move(rows),
                                   "a region's industry_points is outside [0, industry_points_ceiling]");
        }
        total += pts;   // total <= 2^62 before the add and pts <= 2^60: no overflow
        rows.push_back(row);
        if (total > stockpile_points_total_max)
            return rejected_budget(total, std::move(rows),
                                   "the stockpile totals past 2^62 points");
    }
    out.points_total = total;
    if (total == 0)
        return out;   // THE SPAN OFF: no point anywhere, an empty budget, today's world

    // A stock with points and NO carve index at all is not "regions that carved
    // no centre": the index was lost (a snapshot copy or a load keeps the
    // settlement record's pointer but not the index). Rejected whole, as
    // inconsistent, so the loss is loud rather than counted as razing.
    if (founded.empty() && dropped.empty())
        return rejected_budget(total, std::move(rows),
                               "inconsistent: the stockpile holds points but the carve index is empty "
                               "(lost in a copy or a load?)");

    // --- the price (BL-1064, NR-907): the whole stock over the divisor -------
    // Fixed HERE, once, before any point is split, from `total` — every region's
    // points, the ones no centre will take included ("the world's whole
    // industry stockpile"). Integer division floors; a stock smaller than the
    // divisor still prices a charter at 1 point, never at 0. A divisor that is
    // not a divisor, or a price the spend cannot hold, is refused, not clamped.
    if (price_divisor <= 0)
        return rejected_budget(total, std::move(rows),
                               "the price divisor must be > 0 (a charter's price is the stockpile "
                               "over it)");
    const std::int64_t price = std::max<std::int64_t>(1, total / price_divisor);
    if (price > std::numeric_limits<std::int32_t>::max())
        return rejected_budget(total, std::move(rows),
                               "the derived firm price (the stockpile over the divisor) is past int32 "
                               "(the spend's price type)");
    out.firm_price_points = static_cast<std::int32_t>(price);
    out.price_divisor     = price_divisor;

    // --- the carve's slots, per region ----------------------------------------
    // Founded slots in ascending centre id (a std::map), then dropped slots in
    // carve order; each region's list is then sorted by rank, a total order, so
    // the split never depends on how the lists were filled.
    std::map<int, std::vector<slot_ref>> slots;
    const int region_count = static_cast<int>(regions->size());
    for (const auto& [centre, cs] : founded)
    {
        if (cs.region < 0 || cs.region >= region_count)
            return rejected_budget(total, std::move(rows),
                                   "a carved centre names a region outside the settlement record");
        slots[cs.region].push_back({ cs.rank, cs.key, centre, stockpile_unspent_reason::carve_dropped });
    }
    for (const carve_dropped_slot& d : dropped)
    {
        if (d.region < 0 || d.region >= region_count)
            return rejected_budget(total, std::move(rows),
                                   "a dropped carved centre names a region outside the settlement record");
        slots[d.region].push_back({ d.rank, d.key, null_entity,
                                    d.reason == carve_drop_reason::no_tile
                                        ? stockpile_unspent_reason::carve_no_tile
                                        : stockpile_unspent_reason::carve_dropped });
    }
    for (auto& [r, list] : slots)
        std::stable_sort(list.begin(), list.end(), [](const slot_ref& a, const slot_ref& b) {
            if (a.rank != b.rank) return a.rank < b.rank;
            return a.centre < b.centre;
        });

    // --- the split: largest remainder over each region's slots by key --------
    std::map<entity_id, std::int64_t> centre_points;
    for (stockpile_region_row& row : rows)
    {
        const auto it = slots.find(row.region);
        if (it == slots.end() || it->second.empty())
        {
            // NR-901 (Ben 2026-09-19, option A): points on a region whose towns
            // were razed are LOST WITH THEM. A region earns points only while it
            // holds centres, so a point-holding region the carve towns nobody
            // lost its towns after they built; `centres_razed` says history
            // destroyed them. Anything else is the residual, a finding.
            const region& rg = (*regions)[static_cast<std::size_t>(row.region)];
            const stockpile_unspent_reason why = rg.centres_razed > 0
                ? stockpile_unspent_reason::razed
                : stockpile_unspent_reason::no_carved_centre;
            row.unspent[static_cast<std::size_t>(why)] = row.points;
            continue;
        }
        const std::vector<slot_ref>& list = it->second;

        std::vector<std::int64_t> weight(list.size(), 0);
        std::int64_t wsum = 0;
        for (std::size_t k = 0; k < list.size(); ++k)
        {
            if (list[k].key < 0 || list[k].key > stockpile_slot_key_max)
                return rejected_budget(total, std::move(rows),
                                       "a carved slot's key is outside [0, 2^31]");
            weight[k] = list[k].key;
            wsum += weight[k];
            if (wsum > stockpile_region_keys_max)
                return rejected_budget(total, std::move(rows),
                                       "a region's carved slot keys sum past 2^32");
        }
        // A region whose every slot keys to 0 (fewer urban heads than its rank)
        // splits evenly: its slots are still its centres, and the points are
        // still its points.
        if (wsum == 0)
        {
            std::fill(weight.begin(), weight.end(), 1);
            wsum = static_cast<std::int64_t>(weight.size());
        }

        // Staged exactly as BL-1056's `industry_points_apportion_by_scale`:
        // (P / W) * w + ((P % W) * w) / W, with P % W < W <= 2^32 and w <= 2^31.
        const std::int64_t whole = row.points / wsum, part = row.points % wsum;
        std::vector<std::int64_t> share(list.size(), 0), rem(list.size(), 0);
        std::int64_t given = 0;
        for (std::size_t k = 0; k < list.size(); ++k)
        {
            const std::int64_t num = part * weight[k];
            share[k] = whole * weight[k] + num / wsum;
            rem[k]   = num % wsum;
            given   += share[k];
        }
        std::int64_t left = row.points - given;   // 0 <= left < list.size()
        if (left > 0)
        {
            std::vector<std::size_t> order(list.size());
            for (std::size_t k = 0; k < order.size(); ++k) order[k] = k;
            // `list` is in ascending rank, so a stable sort on the remainder
            // alone breaks ties to the lower rank.
            std::stable_sort(order.begin(), order.end(),
                             [&rem](std::size_t a, std::size_t b) { return rem[a] > rem[b]; });
            for (std::size_t k = 0; k < order.size() && left > 0; ++k, --left)
                ++share[order[k]];
        }

        for (std::size_t k = 0; k < list.size(); ++k)
        {
            if (list[k].centre != null_entity)
            {
                centre_points[list[k].centre] += share[k];
                row.to_centres += share[k];
                ++row.founded;
            }
            else
            {
                row.unspent[static_cast<std::size_t>(list[k].drop)] += share[k];
                ++row.dropped;
            }
        }
    }

    // --- the narrowing: a centre's points must fit the budget's int32 --------
    std::map<entity_id, std::int32_t> points;
    for (const auto& [centre, pts] : centre_points)
    {
        if (pts > std::numeric_limits<std::int32_t>::max())
            return rejected_budget(total, std::move(rows),
                                   "a centre's share is past int32 (the charter budget's entry type)");
        points[centre] = static_cast<std::int32_t>(pts);
    }

    out.budget = charter_budget(points);   // a zero share drops, as any budget's does
    // BL-1064: a budget no centre holds a point of (every point razed or
    // dropped) prices nothing — it is today's world, and no price is read on it.
    if (out.budget.empty())
    {
        out.firm_price_points = 0;
        out.price_divisor     = 0;
    }
    else if (region_reach != nullptr)
    {
        // BL-1168 — THE PRICE BY TRADE REACH: each reach's stock is every
        // region's points on it (the ones no centre took included, as the
        // world price counts them), and a centre pays its region's reach's
        // stock over the divisor. A reach's stock is at most the world's, so
        // its price is at most the world's; a centre whose region has no reach
        // pays the world's.
        if (region_reach->size() != regions->size())
            return rejected_budget(total, std::move(rows),
                                   "the region reach keys do not match the settlement record");
        out.reach = reach;
        for (const stockpile_region_row& row : rows)
        {
            const std::int64_t key = (*region_reach)[static_cast<std::size_t>(row.region)];
            if (key >= 0)
                out.reach_stock[key] += row.points;   // <= total <= 2^62: no overflow
        }
        for (const auto& [centre, pts] : out.budget.points())
        {
            (void)pts;
            const auto slot = founded.find(centre);   // every budgeted centre is a founded slot
            const std::int64_t key = (slot != founded.end())
                ? (*region_reach)[static_cast<std::size_t>(slot->second.region)] : -1;
            out.centre_reach[centre] = key;
            std::int64_t p = out.firm_price_points;
            if (key >= 0)
                p = std::max<std::int64_t>(1, out.reach_stock[key] / price_divisor);
            else
                ++out.centres_unreached;
            out.centre_firm_price[centre] = static_cast<std::int32_t>(
                std::min<std::int64_t>(p, out.firm_price_points));
        }
    }
    for (const stockpile_region_row& row : rows)
    {
        out.points_to_centres += row.to_centres;
        for (int r = 0; r < stockpile_unspent_reason_count; ++r)
            out.unspent[static_cast<std::size_t>(r)] += row.unspent[static_cast<std::size_t>(r)];
    }
    out.regions = std::move(rows);
    return out;
}

charter_spend_params stockpile_charter_spend(const stockpile_budget& sb)
{
    charter_spend_params s;
    s.firm_price_points        = sb.firm_price_points;   // derived at build (BL-1064)
    s.centre_firm_price        = sb.centre_firm_price;   // by trade reach (BL-1168)
    s.specialist_firm_charters = k_stockpile_specialist_firm_charters;
    s.window_radius            = 4;
    s.province_cap             = true;
    s.resource_cap_rule        = charter_cap_rule::sqrt_capital;
    s.per_resource_firm_cap    = k_stockpile_per_resource_firm_cap;
    s.max_firms_per_body       = k_stockpile_max_firms_per_body;
    s.density_ceiling          = 0;   // BL-1204: no fixed ceiling; the rate per good served
    s.density_per_good_tenths  = k_stockpile_density_per_good_tenths;
    return s;
}

seed_candidate_spend spend_stockpile_on_seed_candidate(world& w, const recipe_registry& reg,
                                                       std::uint32_t world_seed,
                                                       int corporation_count)
{
    seed_candidate_spend out;
    out.stockpile = build_stockpile_budget(w);
    if (out.stockpile.budget.empty())
        return out;   // the caller's own pre-budget web, untouched

    // The seed candidate, keyed exactly as app::start_new_game_prelude keys the
    // search it starts (sp.start = { count, world_seed ^ 0x8A21F00D, tier 1 })
    // and as tools/verify/harness_params.hpp `shipped_search_params` mirrors it.
    landscape_candidate c;
    c.corporation_count = corporation_count;
    c.placement_seed    = world_seed ^ 0x8A21F00Du;

    // The harness's unsearched apply, verbatim: the recipe pass, the apply's
    // budget overload (whose refusal and NR-910 fallback run inside it), and the
    // second recipe pass a chartered processor needs.
    const charter_spend_params spend = stockpile_charter_spend(out.stockpile);
    assign_default_recipes(w, reg);
    apply_landscape_candidate(w, reg, c, /*regenerate_specialists=*/true,
                              &out.stockpile.budget, spend, &out.report);
    assign_default_recipes(w, reg);
    out.spent = true;
    return out;
}
