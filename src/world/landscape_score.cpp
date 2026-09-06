#include "landscape_score.hpp"

#include "market_clearing.hpp" // market_for_tile — the same catchment partition
#include "recipe_registry.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace
{

/// Coefficient of variation over @p v — the spread measure term 3 rewards.
///
/// CV RATHER THAN VARIANCE, deliberately: it is scale-free, so the spread term
/// cannot be gamed by a candidate that simply scores higher everywhere. A
/// landscape twice as complete in every market is twice as VIABLE and no more
/// UNEVEN, and CV is the statistic that says so.
double coeff_of_variation(const std::vector<double>& v)
{
    if (v.size() < 2)
        return 0.0;                       // one market has no spread to have
    double sum = 0.0;
    for (const double x : v)
        sum += x;
    const double mean = sum / static_cast<double>(v.size());
    if (mean <= 0.0)
        return 0.0;
    double acc = 0.0;
    for (const double x : v)
        acc += (x - mean) * (x - mean);
    return std::sqrt(acc / static_cast<double>(v.size())) / mean;
}

} // namespace

landscape_score score_landscape(world& w, const recipe_registry& reg,
                                const landscape_score_params& p)
{
    return score_landscape(w, reg, classify_resources(w, reg), p);
}

landscape_score score_landscape(world& w, const recipe_registry& reg,
                                const std::array<resource_classification, resource_count>& cls,
                                const landscape_score_params& p)
{
    landscape_score out;

    // Term 1 comes straight from BL-775's promoted measure. TWO CALLERS, ONE
    // IMPLEMENTATION: the census and this scorer read the same completeness, so
    // a candidate cannot score on a number verification does not recognise.
    const std::vector<market_completeness> comp = measure_market_completeness(w, reg, cls);

    const std::vector<entity_id> mids = sorted_market_ids(w);
    std::unordered_map<entity_id, std::size_t> slot;
    out.markets.resize(mids.size());
    for (std::size_t i = 0; i < mids.size(); ++i)
    {
        out.markets[i].market = mids[i];
        slot[mids[i]] = i;
    }
    for (const market_completeness& mc : comp)
    {
        const auto it = slot.find(mc.market);
        if (it == slot.end())
            continue;
        out.markets[it->second].body          = mc.body;
        out.markets[it->second].completeness  = mc.completeness;
    }

    // --- term 2: the static supply:demand ratio, per resource per market -----
    //
    // SUPPLY is deposit magnitude summed over the market's IN-REACH catchment —
    // the ground a building could legally take, not merely ground that clears
    // here. DEMAND is the structural want: heads for a household sink, a flat
    // weight per other market sink. Both are static; neither reads a price.
    //
    // Accumulation is by += over an unordered tile map, which is float addition
    // and therefore NOT associative. The walk is nonetheless deterministic
    // because it is over w.tiles for a FIXED world within one process, and every
    // candidate sees the identical traversal — the property the ranking needs.
    // A cross-process guarantee would want a sorted walk; if this scorer ever
    // ranks across runs, sort by tile id first and say so here.
    const float max_reach = reg.construction().max_logistics_reach;

    std::vector<std::array<double, resource_count>> supply(mids.size());
    for (auto& row : supply)
        row.fill(0.0);
    std::vector<long long> heads(mids.size(), 0);

    for (const auto& [tid, t] : w.tiles)
    {
        const auto it = slot.find(market_for_tile(w, tid));
        if (it == slot.end())
            continue;
        if (max_reach >= 0.0f && !tile_in_reach(w, tid, max_reach))
            continue;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (t.resource_deposit[r] > 0.0f)
                supply[it->second][r] += static_cast<double>(t.resource_deposit[r]);
    }

    for (const auto& [cid, pcc] : w.population_centres)
    {
        if (pcc.razed)
            continue;
        const auto tit = w.population_centre_tile.find(cid);
        if (tit == w.population_centre_tile.end())
            continue;
        const auto it = slot.find(market_for_tile(w, tit->second));
        if (it != slot.end())
            heads[it->second] += static_cast<long long>(pcc.scale);
    }

    for (std::size_t s = 0; s < out.markets.size(); ++s)
    {
        market_score& m = out.markets[s];
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const resource_classification& c = cls[r];
            if (!c.priced)
                continue;                 // untradeable: no price to pin

            double demand = 0.0;
            if (c.sink_household)
                demand += p.household_per_head * static_cast<double>(heads[s]);
            if (c.sink_process)     demand += p.sink_weight;
            if (c.sink_construct)   demand += p.sink_weight;
            if (c.sink_background)  demand += p.sink_weight;
            if (c.sink_unit_upkeep) demand += p.sink_weight;
            if (c.sink_industry)    demand += p.sink_weight;
            if (c.sink_endemic)     demand += p.sink_weight;

            const double sup = supply[s][r];
            if (sup <= 0.0 && demand <= 0.0)
                continue;                 // neither side: the good is absent here

            ++m.rated;
            if (demand <= 0.0)      { ++m.glutted; continue; }  // supply, nobody wants it
            if (sup <= 0.0)         { ++m.starved; continue; }  // wanted, nothing makes it
            const double ratio = sup / demand;
            if (ratio > p.pin_ratio)      ++m.glutted;
            else if (ratio < 1.0 / p.pin_ratio) ++m.starved;
            else                          ++m.balanced;
        }
        m.balance = m.rated > 0 ? static_cast<double>(m.balanced) / m.rated : 0.0;
    }

    // --- the levels, and term 3's spreads -----------------------------------
    std::vector<double> comps, bals;
    comps.reserve(out.markets.size());
    bals.reserve(out.markets.size());
    for (const market_score& m : out.markets)
    {
        comps.push_back(m.completeness);
        bals.push_back(m.balance);
    }
    out.market_count = static_cast<int>(out.markets.size());
    if (!comps.empty())
    {
        double cs = 0.0, bs = 0.0;
        for (std::size_t i = 0; i < comps.size(); ++i) { cs += comps[i]; bs += bals[i]; }
        out.mean_completeness = cs / static_cast<double>(comps.size());
        out.mean_balance      = bs / static_cast<double>(bals.size());
    }
    out.completeness_spread = coeff_of_variation(comps);
    out.balance_spread      = coeff_of_variation(bals);
    out.spread = 0.5 * (out.completeness_spread + out.balance_spread);

    // VIABILITY x UNEVENNESS, and the shape matters. Unevenness is a MULTIPLIER
    // on viability, never an addend: a landscape where nothing works is not
    // rescued by being unevenly broken. That keeps "viable" the necessary
    // condition Ben's point 4 makes it, with "uneven" the tie-break among
    // landscapes that already work.
    const double viability = out.mean_completeness * out.mean_balance;
    out.composite = viability * (1.0 + p.unevenness_gain * out.spread);
    return out;
}
