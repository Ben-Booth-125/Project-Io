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
    // THE TILE WALK IS SORTED BY ID, and that is not optional politeness.
    // Accumulation here is `+=` on a double, which is not associative, and
    // `w.tiles` is an unordered_map — so summing in hash order makes the result
    // depend on bucket layout, which varies with the standard library. This file
    // is in src/world, where the invariant is absolute: no pointer- or
    // hash-layout-dependent iteration order, with no "same process" carve-out.
    // market_saturation.cpp — the sibling this file delegates term 1 to — refuses
    // exactly this and says so at its own tile walk: every accumulation there is
    // an integer increment or a boolean OR, precisely so it needs no sort.
    //
    // The earlier version of this comment argued the unsorted walk was fine
    // because every candidate sees the same traversal within one run. That is
    // true and it is not the standard: a score is a recorded number in a repo
    // whose verification culture is pinned digits, and the same world must not
    // score differently under libstdc++ than under MSVC. Sorting ~31k ids per
    // score is far cheaper than a number nobody can reproduce.
    std::vector<entity_id> tile_ids;
    tile_ids.reserve(w.tiles.size());
    for (const auto& kv : w.tiles)
        tile_ids.push_back(kv.first);
    std::sort(tile_ids.begin(), tile_ids.end());

    const float max_reach = reg.construction().max_logistics_reach;

    std::vector<std::array<double, resource_count>> supply(mids.size());
    for (auto& row : supply)
        row.fill(0.0);
    std::vector<long long> heads(mids.size(), 0);

    for (const entity_id tid : tile_ids)
    {
        const auto it = slot.find(market_for_tile(w, tid));
        if (it == slot.end())
            continue;
        if (max_reach >= 0.0f && !tile_in_reach(w, tid, max_reach))
            continue;
        const tile_component& t = w.tiles.at(tid);
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

    // --- THE ROSTER-AWARE TERM (BL-770 slice 2) -----------------------------
    //
    // WHY IT EXISTS. Slice 1 measured the objective over five candidate rosters
    // on one fixed world and every term came back IDENTICAL TO THE LAST DIGIT,
    // while the fixtures differed by 20 corporations and 41 buildings. The cause
    // was structural, not tuning: completeness, the ratio and the spread all read
    // TILES, MARKETS and POPULATION, and market_saturation.cpp contains neither
    // the word corporation nor building. So the objective measured the WORLD'S
    // saturation potential and was silent on whether any firm realised it —
    // which is exactly the choice phase 6 exists to make.
    //
    // THE FIX IS ACTUAL AGAINST POTENTIAL, and it is deliberately the SAME
    // closure as measure_market_completeness so the two numbers are comparable.
    // Potential seeds the closure from deposits in reach and lets it use every
    // era-allowed recipe. Actual seeds it from what extraction buildings in this
    // catchment REALLY TARGET, and closes it using only the recipes processing
    // buildings here REALLY RUN. Both axes are roster properties: which ground is
    // being worked, and which conversions are staffed.
    {
        const bool processing_available =
            reg.building_available(building_type::processing_facility);
        const int n_allowed = processing_available
                            ? reg.recipe_count(building_type::processing_facility) : 0;

        std::vector<std::array<bool, resource_count>> mined(mids.size());
        for (auto& row : mined) row.fill(false);
        std::vector<std::vector<int>> run(mids.size());

        // Buildings are walked in SORTED id order for the same reason the tile
        // walk is: w.buildings is unordered, and although these accumulations are
        // boolean-OR and push_back, the recipe list's ORDER would otherwise
        // follow bucket layout. The closure below is order-insensitive, but a
        // list whose order varies by standard library is a latent trap for any
        // later reader who assumes otherwise.
        std::vector<entity_id> bids;
        bids.reserve(w.buildings.size());
        for (const auto& kv : w.buildings) bids.push_back(kv.first);
        std::sort(bids.begin(), bids.end());

        for (const entity_id bid : bids)
        {
            const building_component& b = w.buildings.at(bid);
            const auto it = slot.find(market_for_tile(w, b.tile));
            if (it == slot.end())
                continue;
            const std::size_t m = it->second;
            if (b.type == building_type::processing_facility)
            {
                if (b.recipe == no_recipe || b.recipe >= n_allowed)
                    continue;               // configured with nothing: produces nothing
                ++out.markets[m].processors;
                const int rid = static_cast<int>(b.recipe);
                if (std::find(run[m].begin(), run[m].end(), rid) == run[m].end())
                    run[m].push_back(rid);
            }
            else
            {
                ++out.markets[m].extractors;
                mined[m][static_cast<std::size_t>(b.target_resource)] = true;
            }
        }

        const std::array<resource_classification, resource_count>& c2 = cls;
        const std::vector<std::size_t> terms = terminal_resources(c2);

        for (std::size_t m = 0; m < out.markets.size(); ++m)
        {
            std::array<bool, resource_count> have = mined[m];
            bool changed = true;
            while (changed)
            {
                changed = false;
                for (const int i : run[m])
                {
                    const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
                    bool inputs_ok = true;
                    for (std::size_t r = 0; r < resource_count && inputs_ok; ++r)
                        if (rc.inputs[r] > 0.0f && !have[r])
                            inputs_ok = false;
                    if (!inputs_ok)
                        continue;
                    for (std::size_t r = 0; r < resource_count; ++r)
                        if (rc.outputs[r] > 0.0f && !have[r])
                        {
                            have[r] = true;
                            changed = true;
                        }
                }
            }
            int closed = 0;
            for (const std::size_t r : terms)
                if (have[r]) ++closed;
            out.markets[m].actual_closed = closed;
            out.markets[m].actual = terms.empty()
                                  ? 0.0
                                  : static_cast<double>(closed) / static_cast<double>(terms.size());
        }
    }

    // --- the levels, and term 3's spreads -----------------------------------
    std::vector<double> comps, bals, acts;
    comps.reserve(out.markets.size());
    bals.reserve(out.markets.size());
    acts.reserve(out.markets.size());
    for (const market_score& m : out.markets)
    {
        comps.push_back(m.completeness);
        bals.push_back(m.balance);
        acts.push_back(m.actual);
    }
    out.market_count = static_cast<int>(out.markets.size());
    if (!comps.empty())
    {
        double cs = 0.0, bs = 0.0, as_ = 0.0;
        for (std::size_t i = 0; i < comps.size(); ++i)
        { cs += comps[i]; bs += bals[i]; as_ += acts[i]; }
        out.mean_completeness = cs / static_cast<double>(comps.size());
        out.mean_balance      = bs / static_cast<double>(bals.size());
        out.mean_actual       = as_ / static_cast<double>(acts.size());
    }
    out.completeness_spread = coeff_of_variation(comps);
    out.balance_spread      = coeff_of_variation(bals);
    out.spread = 0.5 * (out.completeness_spread + out.balance_spread);

    // VIABILITY x UNEVENNESS, and the shape matters. Unevenness is a MULTIPLIER
    // on viability, never an addend: a landscape where nothing works is not
    // rescued by being unevenly broken. That keeps "viable" the necessary
    // condition Ben's point 4 makes it, with "uneven" the tie-break among
    // landscapes that already work.
    out.realisation = out.mean_completeness > 0.0
                    ? out.mean_actual / out.mean_completeness
                    : 0.0;

    // VIABILITY NOW CARRIES THE ROSTER. mean_actual replaces mean_completeness as
    // the coverage factor: a landscape is viable because its FIRMS close chains,
    // not because its GROUND could. mean_completeness stays on the record as the
    // ceiling that realisation is measured against — the search is choosing among
    // rosters on one fixed world, so the potential is a constant of the world and
    // scoring it would add the same number to every candidate.
    const double viability = out.mean_actual * out.mean_balance;
    out.composite = viability * (1.0 + p.unevenness_gain * out.spread);
    return out;
}
