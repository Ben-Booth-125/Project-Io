#include "stockpile_budget.hpp"

#include "history_sim.hpp"   // industry_points_ceiling
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
    int                     rank   = 0;
    std::int64_t            key    = 0;
    entity_id               centre = null_entity;
    stockpile_unspent_reason drop  = stockpile_unspent_reason::carve_dropped;
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
        r.unspent    = {};
        r.unspent[static_cast<std::size_t>(stockpile_unspent_reason::rejected)] = r.points;
    }
    out.regions = std::move(rows);
    return out;
}

} // namespace

stockpile_budget build_stockpile_budget(const world& w)
{
    stockpile_budget out;
    const settlement_state* ss = w.gen_settlement.get();
    if (ss == nullptr)
        return out;   // a loaded world or a fixture: no stockpile, nothing to account

    // --- the stock, per region ----------------------------------------------
    std::vector<stockpile_region_row> rows;
    std::int64_t total = 0;
    for (std::size_t ri = 0; ri < ss->regions.size(); ++ri)
    {
        const std::int64_t pts = ss->regions[ri].industry_points;
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

    // --- the carve's slots, per region ----------------------------------------
    // Founded slots in ascending centre id (a std::map), then dropped slots in
    // carve order; each region's list is then sorted by rank, a total order, so
    // the split never depends on how the lists were filled.
    std::map<int, std::vector<slot_ref>> slots;
    const int region_count = static_cast<int>(ss->regions.size());
    for (const auto& [centre, cs] : w.gen_carve_centres)
    {
        if (cs.region < 0 || cs.region >= region_count)
            return rejected_budget(total, std::move(rows),
                                   "a carved centre names a region outside the settlement record");
        slots[cs.region].push_back({ cs.rank, cs.key, centre, stockpile_unspent_reason::carve_dropped });
    }
    for (const carve_dropped_slot& d : w.gen_carve_dropped)
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
            row.unspent[static_cast<std::size_t>(stockpile_unspent_reason::no_carved_centre)] = row.points;
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
    for (const stockpile_region_row& row : rows)
    {
        out.points_to_centres += row.to_centres;
        for (int r = 0; r < stockpile_unspent_reason_count; ++r)
            out.unspent[static_cast<std::size_t>(r)] += row.unspent[static_cast<std::size_t>(r)];
    }
    out.regions = std::move(rows);
    return out;
}

charter_spend_params stockpile_charter_spend()
{
    charter_spend_params s;
    s.firm_price_points        = k_stockpile_firm_price_points;
    s.specialist_firm_charters = k_stockpile_specialist_firm_charters;
    s.window_radius            = 4;
    s.province_cap             = true;
    s.resource_cap_rule        = charter_cap_rule::sqrt_capital;
    s.per_resource_firm_cap    = k_stockpile_per_resource_firm_cap;
    s.max_firms_per_body       = k_stockpile_max_firms_per_body;
    s.density_ceiling          = k_stockpile_density_ceiling;
    return s;
}
