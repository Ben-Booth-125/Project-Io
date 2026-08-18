#include "history_sim.hpp"

#include "unit_roster.hpp"

#include <algorithm>

// ---------------------------------------------------------------------------
// The Era -1 history sim (BL-277 + BL-271's first slice). See history_sim.hpp
// for the model; this file is the loop and the scorer.
//
// Everything below is integer arithmetic. There is no RNG: `salt` mixes the
// seed into a polity's weights so polities differ, and into tie-breaks so a
// tie resolves the same way on every replay. That is a hash, not a generator —
// nothing here consumes a stream, so inserting a decision does not shift the
// numbers a later decision sees.
// ---------------------------------------------------------------------------

namespace
{

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

int64_t clampi64(int64_t v, int64_t lo, int64_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

/// Deterministic 32-bit mix (a finaliser, not a generator). Used for weight
/// perturbation and tie-breaks only; never as a probability.
uint32_t salt(uint32_t a, uint32_t b)
{
    uint32_t x = a * 0x9E3779B9u ^ (b + 0x85EBCA6Bu + (a << 6) + (a >> 2));
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

/// Per-polity weight jitter, +/- `spread` per-mille, stable across replays.
///
/// `axis` is what makes polities differ in WHAT THEY VALUE rather than merely
/// in how strongly they value everything at once (BL-312). The first cut mixed
/// only the polity salt against a fixed constant, so a polity that liked
/// farmland 12% above baseline liked ore and ports 12% above baseline too — the
/// relative ordering of the three endowment terms was byte-identical for every
/// polity in every world, which is the opposite of the intended effect.
int jitter(int base, uint32_t s, uint32_t axis, int spread)
{
    if (spread <= 0) return base;
    const int d = static_cast<int>(salt(s, axis) % static_cast<uint32_t>(2 * spread + 1)) - spread;
    return base + (base * d) / 1000;
}

/// THE COMMON CURRENCY (BL-309). Every verb scores in ONE unit: expected
/// annual gain in ENDOWMENT VALUE HELD, where a region is worth the mean of
/// its three endowment windows (0-1000).
///
/// This replaces comparing four incommensurable numbers. The first cut compared
/// raw scores directly, and Invest — whose score was population/4000 clamped —
/// pinned at its ceiling and won forever. The second cut normalised each verb
/// by its own range, which structurally favours the NARROWEST range and simply
/// handed the win to Settle instead. Neither was fixable by tuning, because the
/// error was upstream of the constants: quantities that mean different things
/// cannot be ranked. So each verb now answers the same question — what is this
/// worth to me this year, in regions-worth-of-endowment — and the argmax is
/// an honest comparison rather than a coincidence of scales.
int region_value_q(const region& p)
{
    return (p.farm_q + p.ore_q + p.port_q) / 3;
}

/// What a candidate work is worth this year, IN THE SHARED CURRENCY (BL-321).
///
/// The split between the two terms is the honest part. A Granary feeds THIS
/// region and nowhere else, so it is valued against this region's own
/// endowment. A Way Station shortens marches across the whole polity, so reach
/// is valued against the polity's MEAN holding — mean, not total, because a
/// road through one region does not carry the empire's entire traffic, and
/// scoring it against the total made reach worth ~40x every local effect and
/// reduced the roster to its five road rows.
///
/// The row's authored `weight` then shapes the choice between rows that score
/// alike, as a 0.65x-1.3x multiplier over the table's 130-260 weight range. It
/// is a pull, not a cost — works.lua says so at the point of authoring.
int work_score_q(const work_row&           r,
                 const region&           p,
                 int                       mean_holding_value,
                 const history_sim_params& params)
{
    const work_effect& e = r.effect;

    const int local_q = (e.capacity_mod   * params.w_work_capacity
                       + e.manpower_mod   * params.w_work_manpower
                       + e.defence_mod    * params.w_work_defence
                       + e.industrial_mod * params.w_work_industrial) / 1000;
    const int empire_q = (e.reach_mod * params.w_work_reach) / 1000;

    const int64_t gain = (static_cast<int64_t>(region_value_q(p)) * local_q
                        + static_cast<int64_t>(mean_holding_value)  * empire_q) / 1000;

    const int years = params.work_amortise_years > 0 ? params.work_amortise_years : 1;
    int64_t s = gain / years;
    s = (s * clampi(r.weight, 0, 1000)) / 200;
    return static_cast<int>(clampi64(s, 0, 100000));
}

terrain_composition comp_at(const sim_terrain_view& t, int idx)
{
    if (!t.composition || idx < 0 || idx >= static_cast<int>(t.composition->size()))
        return terrain_composition::grassland;
    return (*t.composition)[static_cast<std::size_t>(idx)];
}

terrain_landform lf_at(const sim_terrain_view& t, int idx)
{
    if (!t.landform || idx < 0 || idx >= static_cast<int>(t.landform->size()))
        return terrain_landform::plains;
    return (*t.landform)[static_cast<std::size_t>(idx)];
}

/// A polity's doctrine, from its culture's aggression (BL-277 Q5). An
/// aggressive creed leans frontal and brittle; a preserving one leans the
/// other way. This is `doctrine_row` as pure data exactly as combat.hpp
/// intends — no new resolution path.
doctrine_row doctrine_for(const polity& p)
{
    doctrine_row d;
    const int a = clampi(p.aggression_q, 0, 1000);
    d.frontal_bonus   = (a - 500) / 3;          // -166 .. +166 per-mille.
    d.flank_fragility = clampi(a / 4, 0, 250);  // Aggression buys exposure.
    d.mountain_penalty = clampi(a / 5, 0, 200);
    d.stance = siege_stance::field;
    return d;
}

/// Turn raised manpower into a typed stack via the era-keyed roster (BL-274),
/// then scale the whole stack by the owner's COHESION (BL-308).
///
/// The roster answers "what can this ground field at this band"; cohesion
/// answers "how well does this polity fight right now". They are separate on
/// purpose: the first is a property of the map, the second of the polity's
/// recent history, and only the second can spiral.
///
/// `readiness_q` is the caller-side lever the winter-campaign candidate uses
/// against a defender (history_sim.hpp § season).
std::vector<army_stack_entry> build_stack(int64_t manpower,
                                          const region& home,
                                          const polity&   owner,
                                          int             readiness_q)
{
    const int band_index = clampi(owner.capacity[static_cast<int>(sim_domain::military)], 1, 6);
    const roster_band band = roster_band_for_capacity(band_index);

    // Cohesion folds into readiness rather than into the counts: a shaken
    // polity fields the same men fighting worse, not fewer men fighting well.
    const int cohesion = clampi(owner.cohesion_q, 0, 1000);
    const int effective_readiness = (readiness_q * cohesion) / 1000;

    return roster_stack(manpower, home, band, effective_readiness);
}

/// Total committed headcount in a stack — the denominator losses apply to.
int64_t stack_size(const std::vector<army_stack_entry>& s)
{
    int64_t n = 0;
    for (const army_stack_entry& e : s) n += e.count;
    return n;
}

} // namespace

// ---------------------------------------------------------------------------

std::vector<uint16_t> owner_slice_at(const history_sim_state& s, int64_t year)
{
    std::vector<uint16_t> slice(static_cast<std::size_t>(s.region_stride), owner_none);
    for (const owner_change& c : s.owner_changes)
    {
        if (static_cast<int64_t>(c.year) > year) break; // Appended in year order.
        if (c.region < slice.size()) slice[c.region] = c.owner;
    }
    return slice;
}

int region_distance(const region& a, const region& b, int gw)
{
    int dc = a.col - b.col;
    if (dc < 0) dc = -dc;
    if (gw > 0 && dc > gw / 2) dc = gw - dc; // Columns wrap: the map is a cylinder.
    int dr = a.row - b.row;
    if (dr < 0) dr = -dr;
    return dc > dr ? dc : dr; // Chebyshev — movement is eight-connected.
}

int step_for_year(const history_sim_params& p, int64_t y)
{
    const int n = clampi(p.tick_band_count, 0, sim_tick_band_max);
    if (n <= 0)
        return 1; // No table: fall back to the flat year tick.

    for (int i = 0; i < n; ++i)
        if (y < p.tick_bands[i].until_year)
            return p.tick_bands[i].step_years > 0 ? p.tick_bands[i].step_years : 1;

    // Past the last boundary — the finest band governs the tail. This is the
    // ordinary case for the last band, whose `until_year` IS the stop year.
    const int last = p.tick_bands[n - 1].step_years;
    return last > 0 ? last : 1;
}

// ---------------------------------------------------------------------------

history_sim_state run_history_sim(settlement_state&         ss,
                                  const creed_state*        cs,
                                  const sim_terrain_view&   terrain,
                                  int                       gw,
                                  int                       gh,
                                  const history_sim_params& params,
                                  uint32_t                  seed,
                                  std::atomic<int>*         year_progress,
                                  const works_registry*     works)
{
    history_sim_state out;
    if (ss.regions.empty() || params.stop_year <= params.start_year)
        return out;

    // --- Seed polities from cultures --------------------------------------
    //
    // At the antiquity start `region::nation` is -1: the political pass has
    // not run, and a pre-national world's actors ARE its peoples (BL-221). The
    // sim writes `nation` as it goes, so the political map is this loop's
    // output rather than its input.
    {
        std::vector<int> cultures;
        for (const region& p : ss.regions)
            if (p.culture >= 0 && std::find(cultures.begin(), cultures.end(), p.culture) == cultures.end())
                cultures.push_back(p.culture);
        std::sort(cultures.begin(), cultures.end()); // Order must not depend on placement.

        for (int c : cultures)
        {
            polity q;
            q.id      = static_cast<int>(out.polities.size());
            q.culture = c;
            if (cs && c < static_cast<int>(cs->cultures.size()))
                q.aggression_q = cs->cultures[static_cast<std::size_t>(c)].aggression_q;
            else
                q.aggression_q = 500; // Neutral when creeds were not supplied.

            // Capital: the best-settled region of this culture. Ties break on
            // the lower index, which is placement order (best ground first).
            int best = -1, best_q = -1;
            for (std::size_t i = 0; i < ss.regions.size(); ++i)
            {
                const region& p = ss.regions[i];
                if (p.culture != c) continue;
                if (p.settle_score_q > best_q) { best_q = p.settle_score_q; best = static_cast<int>(i); }
            }
            q.capital = best;
            out.polities.push_back(q);
        }
    }
    if (out.polities.empty()) return out;

    // --- Great-power seed (BL-299) ----------------------------------------
    //
    // Two majors with OPPOSED strategic creeds — one preserving, one on a
    // civilising mission — in a world whose periphery stays multipolar. The
    // periphery is not terrain: minors keep their own cultures, doctrines and
    // recorded history, exactly as the item's richness clause requires. All
    // this seed does is set two aggressions apart and mark the pair.
    if (params.seed_great_powers && out.polities.size() >= 2)
    {
        // Largest two by starting holdings, ties to the lower id so the choice
        // does not depend on iteration order.
        std::vector<std::pair<int, int>> by_size; // (count, id)
        for (const polity& q : out.polities)
        {
            int n = 0;
            for (const region& p : ss.regions) if (p.culture == q.culture) ++n;
            by_size.push_back({n, q.id});
        }
        std::sort(by_size.begin(), by_size.end(),
                  [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                      if (a.first != b.first) return a.first > b.first;
                      return a.second < b.second;
                  });

        polity& expansionist = out.polities[static_cast<std::size_t>(by_size[0].second)];
        polity& preserving   = out.polities[static_cast<std::size_t>(by_size[1].second)];
        expansionist.major = true;
        preserving.major   = true;
        expansionist.aggression_q = clampi(params.major_expansionist_aggression_q, 0, 1000);
        preserving.aggression_q   = clampi(params.major_preserving_aggression_q, 0, 1000);
    }

    // Region -> owning polity. Seeded from culture, then owned by conquest.
    std::vector<int> owner(ss.regions.size(), -1);
    for (std::size_t i = 0; i < ss.regions.size(); ++i)
        for (const polity& q : out.polities)
            if (q.culture == ss.regions[i].culture) { owner[i] = q.id; break; }

    for (std::size_t i = 0; i < ss.regions.size(); ++i)
    {
        region& p = ss.regions[i];
        p.nation = owner[i];
        // Seed a headcount so demography has something to grow from — the
        // graduation path settlement.hpp's demography note leaves to this item.
        if (p.population <= 0)
            p.population = clampi64(region_carrying_capacity(p.farm_q) / 8, 1, 1 << 30);
        p.last_demography_year = params.start_year;
        replenish_manpower(p);
    }

    // --- Per-year war pressure, reset each tick ---------------------------
    std::vector<int> war_pressure(ss.regions.size(), 0);

    // --- Neighbour index --------------------------------------------------
    //
    // Campaign candidates are NEIGHBOURS ONLY, so the neighbourhood is built
    // once rather than rediscovered by scanning every region from every held
    // region every year. That scan is quadratic in region count and, with
    // the Settle verb growing the map past 400 regions, it dominated the
    // whole run (2.5s of a 2.5s run). Built once here, extended when a
    // region is founded, it is a lookup.
    std::vector<std::vector<int>> neighbours(ss.regions.size());
    const auto link_region = [&](std::size_t i) {
        for (std::size_t j = 0; j < ss.regions.size(); ++j)
        {
            if (i == j) continue;
            if (region_distance(ss.regions[i], ss.regions[j], gw) <= params.neighbour_radius)
            {
                neighbours[i].push_back(static_cast<int>(j));
                neighbours[j].push_back(static_cast<int>(i));
            }
        }
    };
    for (std::size_t i = 0; i < ss.regions.size(); ++i)
        for (std::size_t j = i + 1; j < ss.regions.size(); ++j)
            if (region_distance(ss.regions[i], ss.regions[j], gw) <= params.neighbour_radius)
            {
                neighbours[i].push_back(static_cast<int>(j));
                neighbours[j].push_back(static_cast<int>(i));
            }

    // --- Terrain-weighted reach (BL-314 S2) -------------------------------
    //
    // Distance from the capital is a COST over the neighbour graph, not a
    // straight line: crossing a mountain range costs about twice what crossing
    // plains does, using the landform ratios logistics.cpp already defines for
    // the 1960 era. Computed by Dijkstra from the capital and cached until the
    // capital moves, so the per-year cost stays a lookup.
    std::vector<int> reach;            // Per-region cost from the current capital.
    int reach_capital = -2;            // Which capital `reach` was built for.

    const auto tile_cost = [&](const region& p) {
        // Landform ratios, x100: plains 100, highland 125, mountain 200, ...
        switch (lf_at(terrain, p.anchor))
        {
        case terrain_landform::mountain: return 200;
        case terrain_landform::rift:     return 160;
        case terrain_landform::canyon:   return 150;
        case terrain_landform::crater:   return 130;
        case terrain_landform::highland: return 125;
        case terrain_landform::valley:   return 110;
        default:                         return 100;
        }
    };

    const auto rebuild_reach = [&](int capital) {
        reach.assign(ss.regions.size(), 1 << 28);
        if (capital < 0 || capital >= static_cast<int>(ss.regions.size())) return;
        reach[static_cast<std::size_t>(capital)] = 0;

        // Dijkstra without a heap: region counts are hundreds, and a simple
        // scan keeps the order deterministic without depending on a tie-break
        // inside a priority queue.
        std::vector<bool> done(ss.regions.size(), false);
        for (std::size_t iter = 0; iter < ss.regions.size(); ++iter)
        {
            int best = -1, best_c = 1 << 28;
            for (std::size_t i = 0; i < reach.size(); ++i)
                if (!done[i] && reach[i] < best_c) { best_c = reach[i]; best = static_cast<int>(i); }
            if (best < 0) break;
            done[static_cast<std::size_t>(best)] = true;

            const region& bp = ss.regions[static_cast<std::size_t>(best)];
            for (int nb : neighbours[static_cast<std::size_t>(best)])
            {
                const region& np2 = ss.regions[static_cast<std::size_t>(nb)];
                const int step = region_distance(bp, np2, gw)
                               * (tile_cost(bp) + tile_cost(np2)) / 200;
                const int cand = best_c + (step > 0 ? step : 1);
                if (cand < reach[static_cast<std::size_t>(nb)])
                    reach[static_cast<std::size_t>(nb)] = cand;
            }
        }
        reach_capital = capital;
    };

    // --- Time-lapse change list -------------------------------------------
    out.owner_changes.clear();
    for (std::size_t i = 0; i < owner.size(); ++i)
        if (owner[i] >= 0)
            out.owner_changes.push_back(owner_change{
                static_cast<int32_t>(params.start_year),
                static_cast<uint16_t>(i),
                static_cast<uint16_t>(owner[i])});

    const int64_t years = params.stop_year - params.start_year;
    out.battles_per_century.assign(static_cast<std::size_t>(years / 100 + 1), 0);

    // The stepped decision clock (Ben, 2026-08-12). `y` still advances one real
    // year at a time — demography must not skip — but the polities only ACT
    // when the year reaches `next_decision`, which walks forward by the current
    // band's step. See history_sim.hpp § The stepped decision clock.
    int64_t next_decision = params.start_year;
    int     step_years    = step_for_year(params, params.start_year);

    for (int64_t y = params.start_year; y < params.stop_year; ++y)
    {
        // Loading-screen sink only — never read back, so the sim stays pure.
        if (year_progress != nullptr)
            year_progress->store(static_cast<int>(y - params.start_year + 1),
                                 std::memory_order_relaxed);

        const std::size_t century =
            static_cast<std::size_t>((y - params.start_year) / 100);

        // ---- Demography -------------------------------------------------
        int64_t total_pop = 0;
        for (std::size_t i = 0; i < ss.regions.size(); ++i)
        {
            advance_region_demography(ss.regions[i], 1, war_pressure[i]);
            war_pressure[i] = 0;
            total_pop += ss.regions[i].population;
        }
        if (total_pop > out.peak_population)
        {
            out.peak_population = total_pop;
            out.peak_year       = y;
        }

        // ---- The decision gate -------------------------------------------
        //
        // Below the band's step there is nothing for the polities to do this
        // year: demography above has already run, and the scorer — the
        // expensive half of this loop — is deliberately coarse in deep
        // prehistory. `step_years` is read here and used by the three RATE
        // applications further down (tech progress, cohesion recovery, contest
        // decay), which must cover the whole interval since the last round.
        if (y < next_decision)
            continue;
        step_years    = step_for_year(params, y);
        next_decision = y + step_years;

        // ---- Each polity acts, in id order (deterministic) ---------------
        for (polity& q : out.polities)
        {
            if (!q.alive) continue;

            // Holdings, and whether the capital still stands.
            std::vector<int> held;
            for (std::size_t i = 0; i < owner.size(); ++i)
                if (owner[i] == q.id) held.push_back(static_cast<int>(i));

            if (held.empty()) { q.alive = false; continue; }
            if (q.capital < 0 || owner[static_cast<std::size_t>(q.capital)] != q.id)
                // Capital fell. The successor is the polity's lowest-indexed
                // surviving region — placement order, which is best-ground
                // first, so it is a reasonable seat without being "the largest
                // holding" the first cut's comment claimed (BL-312).
                q.capital = held.front();

            const region& cap = ss.regions[static_cast<std::size_t>(q.capital)];
            const uint32_t  qs  = salt(seed, static_cast<uint32_t>(q.id));

            if (reach_capital != q.capital || reach.size() != ss.regions.size())
                rebuild_reach(q.capital);

            // ---- What this polity's WORKS are worth it (BL-321) -----------
            //
            // One pass over `held`, producing the three aggregates the round
            // needs: the mean holding value (the denominator a work's
            // empire-wide benefit is scored against), the mean reach investment
            // (what relieves the burden of breadth), and the mean industrial
            // investment (what accelerates the tech ladder).
            //
            // MEANS, NOT TOTALS, throughout. A total would make every aggregate
            // grow with conquest alone, so a large empire would count itself as
            // well-roaded for having many unroaded regions — the opposite of
            // what the burden of breadth is measuring.
            int64_t holdings_value_sum = 0;
            int64_t works_reach_sum    = 0;
            int64_t works_ind_sum      = 0;
            for (int hi : held)
            {
                const region& hp = ss.regions[static_cast<std::size_t>(hi)];
                holdings_value_sum += region_value_q(hp);
                works_reach_sum    += hp.work_reach_mod;
                works_ind_sum      += hp.work_industrial_mod;
            }
            const int n_held = static_cast<int>(held.size()); // >= 1: empty held returned above.
            const int mean_holding_value = static_cast<int>(holdings_value_sum / n_held);
            const int mean_reach_q       = static_cast<int>(works_reach_sum / n_held);
            const int mean_industrial_q  = static_cast<int>(works_ind_sum / n_held);

            // THE BURDEN OF BREADTH (BL-314 S3). Every region held past
            // `free_holdings` costs supply on every campaign this polity runs.
            const int over = static_cast<int>(held.size()) - params.free_holdings;
            int burden = over > 0 ? over * params.holdings_burden_q : 0;

            // AND THE COUNTER-MOVE (BL-321). A polity that spent its rounds on
            // roads and wharves administers its breadth more cheaply. Relief is
            // proportional and capped below 1000, so building buys a discount
            // and never an exemption: the stall still arrives, just later and
            // by the polity's own choice rather than by arithmetic it could do
            // nothing about. That difference — ceiling to decision — is the
            // whole reason this item exists.
            {
                const int relief = clampi(mean_reach_q, 0, params.work_reach_relief_cap_q);
                burden = burden - (burden * relief) / 1000;
            }

            // ONE PRICE FOR SUPPLY, PAID BY BOTH THE SCORER AND THE BATTLE
            // (BL-318). These were two separate expressions, and they disagreed:
            // the scorer measured the staging hub's distance (bounded by
            // neighbour_radius, so at most ~250 per-mille of decay) while the
            // executed battle re-derived it from the CAPITAL (unbounded, and on
            // a 312-wide map routinely past the 1000 that clamps supply to
            // zero). So the scorer chose campaigns it believed were nearly fully
            // supplied and then fought them at nothing, which is why
            // `stalled_campaigns` counted almost every launch.
            //
            // That is this item's whole thesis in miniature — a cost authored on
            // one scale and spent on another — so the fix is not to pick the
            // better of the two lines but to delete one of them. Priced here,
            // once, the estimate and the outcome cannot drift again.
            //
            // The three terms are the three costs the design names: LOCAL
            // staging distance, STRATEGIC terrain-weighted reach from the
            // capital (BL-316 S2), and the BURDEN OF BREADTH (BL-316 S3).
            // `hub` is the region the campaign is STAGED FROM, and it is a
            // parameter rather than a capture because its works discount the
            // terrain cost (BL-321): reach_mod is authored as a discount on the
            // supply cost through/from a region, so it is the staging
            // holding's roads and wharves doing the carrying, not the capital's.
            //
            // The discount applies to the TERRAIN term alone. A span bridge
            // makes a mountain cheaper to cross; it does not shorten the march,
            // which is what supply_decay_per_tile_q charges for.
            const auto campaign_supply = [&](int hub_dist, std::size_t ti, int hub) {
                const int reach_here = (ti < reach.size() && reach[ti] < (1 << 27))
                                     ? static_cast<int>(reach[ti]) : hub_dist;
                const int hub_reach_q = (hub >= 0)
                    ? clampi(ss.regions[static_cast<std::size_t>(hub)].work_reach_mod,
                             0, params.work_reach_relief_cap_q)
                    : 0;
                const int terrain_cost = reach_here * params.terrain_reach_cost_q / 100;
                const int terrain_paid = terrain_cost - (terrain_cost * hub_reach_q) / 1000;
                return clampi(1000
                            - hub_dist * params.supply_decay_per_tile_q
                            - terrain_paid
                            - clampi(burden, 0, 1000 - params.holdings_burden_floor_q),
                            0, 1000);
            };

            // ---- Build the bounded candidate set -------------------------
            //
            // Four verbs. The set is bounded by construction: neighbours only,
            // one settle site, one invest domain, one consolidate.
            sim_verb best_verb  = sim_verb::none;
            int      best_score = 0;
            int      best_target = -1;
            bool     best_winter = false;
            // Which works row `build_work` chose. A second field rather than an
            // overload of `best_target`, because build_work needs BOTH a
            // region and a row and the other verbs' single target already
            // means three different things (region, parent, domain).
            int      best_work_row = -1;

            // -- Campaign --------------------------------------------------
            for (int hi : held)
            {
                for (int tn : neighbours[static_cast<std::size_t>(hi)])
                {
                    const std::size_t ti = static_cast<std::size_t>(tn);
                    const int to = owner[ti];
                    if (to == q.id || to < 0) continue;

                    const region& tgt = ss.regions[ti];
                    const int cap_dist = region_distance(cap, tgt, gw);

                    // SUPPLY PROJECTS FROM THE STAGING HOLDING, NOT THE CAPITAL
                    // (Ben, 2026-08-12: "perhaps we can try instructing supply
                    // hubs?").
                    //
                    // `hi` is already the polity's own region adjacent to the
                    // target — it IS a supply hub, and the loop was standing in
                    // it while measuring supply all the way back to the capital.
                    // That made reach a property of empire SHAPE rather than of
                    // frontier presence: a large polity could not attack its own
                    // border because its capital was far away.
                    //
                    // Measuring from the staging region instead is both the
                    // better model (armies victual at the frontier) and what
                    // makes reach scale-free — hub_dist is bounded by
                    // neighbour_radius, so it cannot blow up when the map does.
                    // The capital still matters, as the preference term below.
                    const int hub_dist =
                        region_distance(ss.regions[static_cast<std::size_t>(hi)], tgt, gw);

                    // Defender's fielded power, as a headcount proxy, raised by
                    // whatever the target has BUILT (BL-321). A Wall Circuit
                    // has to be visible to the scorer, not only to
                    // resolve_battle: a polity that walked into a bastion it
                    // could not see would be making the decision on stale
                    // information every time, and the work would read as bad
                    // luck rather than as the defender's choice it is.
                    const int64_t def_men = (tgt.manpower_stock * params.levy_fraction_q) / 1000;
                    const int def_works   = clampi(tgt.work_defence_mod, 0, 1000);
                    const int def_scaled  = static_cast<int>(clampi64(
                        (def_men / 64) * (1000 + def_works) / 1000, 0, 1000));

                    // Three DIFFERENT axes, so a polity can prize farmland and
                    // shrug at ore rather than merely valuing everything alike.
                    int value = (jitter(params.w_farm, qs, 0xA1u, 160) * tgt.farm_q
                              +  jitter(params.w_ore,  qs, 0xB2u, 160) * tgt.ore_q
                              +  jitter(params.w_port, qs, 0xC3u, 160) * tgt.port_q) / 1000;

                    // RING CLOSURE — the mechanical answer to "what makes a
                    // Rome" (BL-277 Q1). A coastal target next to coast this
                    // polity already holds advances the ring, so littoral
                    // hegemony emerges from a scored term, never a script.
                    if (tgt.port_q > 400)
                    {
                        int coastal_held = 0;
                        for (int h2 : held)
                            if (ss.regions[static_cast<std::size_t>(h2)].port_q > 400
                             && region_distance(ss.regions[static_cast<std::size_t>(h2)], tgt, gw)
                                    <= params.neighbour_radius)
                                ++coastal_held;
                        const int ring_closure_q = clampi(coastal_held * 250, 0, 1000);
                        value += (params.w_ring * ring_closure_q) / 1000;
                    }

                    // In the currency: expected value TAKEN, discounted by the
                    // odds of taking it, less what the attempt costs.
                    value = (value * params.campaign_gain_q) / 1000;

                    // Odds from the power ratio the sim can actually estimate:
                    // levy x supply x cohesion against the defender's levy.
                    const int supply_here = campaign_supply(hub_dist, ti, hi);

                    int64_t atk_men = 0;
                    for (int hi2 : held)
                        if (ss.regions[static_cast<std::size_t>(hi2)].manpower_stock > atk_men)
                            atk_men = ss.regions[static_cast<std::size_t>(hi2)].manpower_stock;
                    const int64_t atk_est = (atk_men * supply_here / 1000)
                                          * clampi(q.cohesion_q, 1, 1000) / 1000;
                    const int64_t def_est = tgt.manpower_stock > 0 ? tgt.manpower_stock : 1;
                    const int p_win_q = static_cast<int>(
                        clampi64((atk_est * 1000) / (atk_est + def_est), 0, 1000));

                    value = (value * p_win_q) / 1000;

                    // Costs, in the same unit: distance is a real logistics
                    // cost now (BL-314), not just a preference.
                    // DISTANCE IS A PREFERENCE HERE, NOT THE COST — and it must
                    // be proportional for the same reason w_cult had to be
                    // (2026-08-12).
                    //
                    // This was `value -= w_dist * cap_dist / 10`, flat. It was
                    // survivable on a 180-wide map and fatal on a 312-wide one:
                    // capital-to-frontier distances scale with the map, region
                    // values do not, so tripling the grid tripled this penalty
                    // against an unchanged prize and drove every campaign score
                    // below threshold. Measured on the real world: 0 battles,
                    // 1195 foundings — a completely peaceful 400 years.
                    //
                    // The REAL cost of distance is already modelled, twice, and
                    // properly: supply_decay_per_tile_q and terrain_reach_cost_q
                    // both feed `supply_here` below, which feeds both the odds
                    // and an explicit supply cost. This term only ever expressed
                    // "nearer is nicer", so as a per-mille discount it says that
                    // at any map size instead of vetoing war on large ones.
                    value = (value * clampi(1000 - params.w_dist * cap_dist / 100, 200, 1000)) / 1000;
                    value -= (1000 - supply_here) * params.campaign_supply_cost_q / 1000;
                    // FOREIGN GROUND IS WORTH LESS, PROPORTIONALLY — not a flat
                    // toll (2026-08-12).
                    //
                    // This was `value -= w_cult`, a flat 150 subtracted from a
                    // region value that measures only ~200-300. It therefore
                    // ate half to three quarters of the entire prize on every
                    // cross-cultural target, which is nearly all of them, and it
                    // alone suppressed EVERY war: measured across a 400-year run,
                    // w_cult 150 -> 0 battles, and 0 -> 266 battles / 150
                    // conquests, with w_dist and w_def making no difference at
                    // all. A term meant to express a preference was acting as a
                    // veto.
                    //
                    // That is BL-318's incommensurability in miniature: a cost
                    // authored on one scale subtracted from a value on another.
                    // As a fraction it means the same thing at any region
                    // value — foreign ground is worth (1000 - w_cult)/1000 of
                    // what home ground is — so the signal survives and the veto
                    // does not.
                    if (tgt.culture != q.culture)
                        value = (value * (1000 - clampi(params.w_cult, 0, 1000))) / 1000;

                    // Season as an action axis: summer and winter are two
                    // candidates over the same objective, not two ticks.
                    //
                    // Winter's BENEFIT must appear in the score, not only at
                    // execution. Scoring winter with the same defender power as
                    // summer and then charging it a premium makes it strictly
                    // worse than summer, so it is never chosen and the whole
                    // axis is dead — the first cut had exactly that bug. The
                    // defender's readiness penalty is what the scorer is
                    // trading the premium and the extra attrition against.
                    for (int w = 0; w < 2; ++w)
                    {
                        const bool winter = (w == 1);
                        const int def_ready = winter ? (1000 - params.winter_readiness_penalty_q) : 1000;
                        const int def_eff   = (def_scaled * def_ready) / 1000;

                        int s = value - (params.w_def * def_eff) / 2000;
                        if (winter) s -= params.winter_score_premium_q;
                        s += static_cast<int>(salt(qs, static_cast<uint32_t>(ti)) % 16u); // Stable tie-break.

                        if (s > best_score && s >= params.campaign_threshold_q)
                        {
                            best_score = s; best_verb = sim_verb::campaign;
                            best_target = static_cast<int>(ti); best_winter = winter;
                        }
                    }
                }
            }

            // -- Settle ----------------------------------------------------
            //
            // The growth-without-war axis. `run_settlement` founds every
            // region before the stop year and leaves none after it, so
            // without this verb a 2000-year run has a frozen region count.
            {
                int pressure_best = -1, pressure_src = -1;
                for (int hi : held)
                {
                    const region& p = ss.regions[static_cast<std::size_t>(hi)];
                    // The works-aware ceiling (BL-321), matching what
                    // `advance_region_demography` actually grows toward. The
                    // plain overload would have read a Granary region as far
                    // more crowded than it is and sent it out to settle land it
                    // did not need — the settle pressure and the growth model
                    // must divide by the same K or the verb fires on a fiction.
                    const int64_t K = region_carrying_capacity(p.farm_q, p.work_capacity_mod);
                    if (K <= 0) continue;
                    const int pressure = static_cast<int>(clampi64((p.population * 1000) / K, 0, 1000));
                    if (pressure > pressure_best) { pressure_best = pressure; pressure_src = hi; }
                }
                // A polity fighting for its life does not colonise (BL-308).
                // Letting it was the main reason losers regrew faster than they
                // were conquered, and why elimination never happened.
                const bool may_settle = q.cohesion_q >= params.settle_cohesion_gate_q;
                if (may_settle && pressure_src >= 0 && pressure_best >= params.settle_pressure_q)
                {
                    const region& sp = ss.regions[static_cast<std::size_t>(pressure_src)];
                    const int daughter_value = (region_value_q(sp) * 800) / 1000;
                    const int s = (daughter_value * pressure_best) / 1000
                                - (static_cast<int>(held.size()) > params.free_holdings
                                   ? params.holdings_burden_q * 4 : 0);
                    if (s > best_score && s >= params.settle_threshold_q)
                    {
                        best_score = s; best_verb = sim_verb::settle; best_target = pressure_src;
                        best_winter = false;
                    }
                }
            }

            // -- Invest ----------------------------------------------------
            {
                // One domain: the lowest band, ties to the lower enum value.
                int dom = 0;
                for (int d = 1; d < sim_domain_count; ++d)
                    if (q.capacity[d] < q.capacity[dom]) dom = d;

                int64_t pop = 0;
                for (int hi : held) pop += ss.regions[static_cast<std::size_t>(hi)].population;

                // Divided by the band already held: each further band costs more
                // to want, so investment does not pin at its ceiling the moment
                // a polity has a few mature regions (BL-309). Marginal value,
                // not accumulated size.
                int64_t holdings_value = 0;
                for (int hi : held)
                    holdings_value += region_value_q(ss.regions[static_cast<std::size_t>(hi)]);
                const int band_now = clampi(q.capacity[dom], 1, 6);
                const int s = static_cast<int>(clampi64(
                    (holdings_value * params.invest_yield_q)
                        / (1000LL * params.invest_amortise_years * band_now),
                    0, 1000));
                (void)pop;
                if (s > best_score && s >= params.invest_threshold_q)
                {
                    best_score = s; best_verb = sim_verb::invest; best_target = dom;
                    best_winter = false;
                }
            }

            // -- Consolidate -----------------------------------------------
            //
            // A scored candidate, not merely the fallback. It is worth more to a
            // polity whose cohesion has been dented, which is what makes the
            // BL-308 death-spiral escape reachable at all: under raw-score
            // comparison Consolidate was never chosen after ~year 176, so the
            // recovery the header promised could not happen.
            {
                int64_t hv = 0;
                for (int hi : held)
                    hv += region_value_q(ss.regions[static_cast<std::size_t>(hi)]);
                const int shortfall = 1000 - clampi(q.cohesion_q, 0, 1000);
                const int s = static_cast<int>(clampi64(
                    (hv * shortfall) / (1000LL * params.consolidate_divisor), 0, 1000));
                if (s > best_score && s >= params.consolidate_threshold_q)
                {
                    best_score = s; best_verb = sim_verb::consolidate;
                    best_target = -1; best_winter = false;
                }
            }
            // -- Build a work (BL-321) -------------------------------------
            //
            // Scored LAST, and that ordering carries a small contract: because
            // every earlier verb uses `>` against `best_score`, a work must
            // strictly beat them, and `best_work_row` is only ever read on the
            // one path that just wrote it.
            //
            // BOUNDED BY CONSTRUCTION, like the other four. Two candidate
            // regions — the capital, and one rotated through the holdings by
            // the year — rather than every holding. Scoring all of them would be
            // O(held x rows) per polity per round inside a pass already costing
            // ~23 s of a ~25 s world; the rotation still reaches every region
            // over a run, because the round count (136 on the default ladder) is
            // large against any one polity's holdings.
            if (works != nullptr && works->size() > 0)
            {
                // Keyed off MATERIALS, not military. The unit roster reads the
                // military column because that is the column whose rows turn
                // over at a roster boundary; a Blast Works turns over with
                // metallurgy instead. Same band enum, different column — which
                // is the point of the two tables sharing `roster_band` rather
                // than one deriving from the other.
                const roster_band band =
                    roster_band_for_capacity(clampi(q.capacity[static_cast<int>(sim_domain::materials)], 1, 6));

                for (int slot = 0; slot < clampi(params.work_candidate_regions, 0, 8); ++slot)
                {
                    int pi = -1;
                    if (slot == 0)
                    {
                        pi = q.capital;
                    }
                    else
                    {
                        // Rotation is a hash of (polity, year, slot), not a
                        // counter: nothing is carried between rounds, so
                        // inserting or removing a decision cannot shift which
                        // region a later round looks at. Same reason the rest
                        // of this file uses `salt` rather than a generator.
                        const uint32_t h = salt(qs, static_cast<uint32_t>(y) * 977u
                                                    + static_cast<uint32_t>(slot));
                        pi = held[static_cast<std::size_t>(h % static_cast<uint32_t>(n_held))];
                    }
                    if (pi < 0 || pi >= static_cast<int>(ss.regions.size())) continue;

                    const region& bp = ss.regions[static_cast<std::size_t>(pi)];
                    const std::vector<const work_row*> avail = works->available(bp, band);

                    for (const work_row* r : avail)
                    {
                        const int id = works->index_of(r);
                        if (id < 0 || static_cast<std::size_t>(id) >= works_mask_bits) continue;
                        // Already standing here: a work is a finite, saturating
                        // investment, not a dial a polity can keep turning.
                        if ((bp.works_built & (uint32_t{1} << id)) != 0) continue;

                        int s = work_score_q(*r, bp, mean_holding_value, params);
                        s += static_cast<int>(salt(qs, static_cast<uint32_t>(id) * 31u
                                                      + static_cast<uint32_t>(pi))
                                              % 8u); // Stable tie-break, as elsewhere.

                        if (s > best_score && s >= params.work_threshold_q)
                        {
                            best_score = s; best_verb = sim_verb::build_work;
                            best_target = pi; best_work_row = id; best_winter = false;
                        }
                    }
                }
            }

            if (best_verb == sim_verb::none) best_verb = sim_verb::consolidate;

            // ---- Execute -------------------------------------------------
            switch (best_verb)
            {
            case sim_verb::campaign:
            {
                const std::size_t ti = static_cast<std::size_t>(best_target);
                region& tgt = ss.regions[ti];

                // Nearest holding is the staging region.
                int src = held.front(), src_d = 1 << 30;
                for (int hi : held)
                {
                    const int d = region_distance(ss.regions[static_cast<std::size_t>(hi)], tgt, gw);
                    if (d < src_d) { src_d = d; src = hi; }
                }
                region& home = ss.regions[static_cast<std::size_t>(src)];

                const int64_t want   = (home.manpower_stock * params.levy_fraction_q) / 1000;
                const int64_t raised = raise_manpower(home, want);
                if (raised <= 0) break;

                // FORCE COMMITMENT (BL-277 Q2), priced by the SAME lambda the
                // scorer used. `src_d` is the staging hub's distance — the army
                // victuals at the frontier region it marched from, and the
                // capital still bears on the result through the terrain-weighted
                // reach term inside `campaign_supply`.
                //
                // Execute picks the NEAREST holding while the scorer scored one
                // (hub, target) pair, so the fought supply is never worse than
                // the scored estimate. Optimistic by a bounded amount, and in
                // the right direction: the sim does not launch campaigns it then
                // silently under-supplies.
                const int atk_supply = campaign_supply(src_d, ti, src);
                const int def_supply = 1000;

                // The stall, counted where it actually happens (BL-312). The
                // first cut incremented this AFTER resolve_battle and only at
                // exactly zero supply, so it counted launched battles rather
                // than stalls and measured a constant zero in every run.
                if (atk_supply < params.stalled_supply_q) ++out.stalled_campaigns;

                // THE DEFENCE WORKS LAND HERE (BL-321), as readiness on the
                // defender's stack — which `roster_stack` turns into an
                // additive per-mille offset on each unit's `type_power_mod`.
                // That is deliberately the same channel cohesion uses, and for
                // the same reason combat.hpp gives: the engine scores whatever
                // stack it is handed and knows nothing about walls. A Bastion
                // Fort at +640 is worth about +64 against row power values of
                // 90..380 — it tilts a fight rather than deciding one, so a
                // fortress buys the defender an edge and never immunity.
                const int def_works_q = clampi(tgt.work_defence_mod, 0, 1000);
                const int def_ready = (best_winter ? (1000 - params.winter_readiness_penalty_q) : 1000)
                                    + def_works_q;

                const polity* dq = nullptr;
                for (const polity& o : out.polities)
                    if (o.id == owner[ti]) { dq = &o; break; }

                std::vector<army_stack_entry> atk = build_stack(raised, home, q, 1000);
                const int64_t def_want = (tgt.manpower_stock * params.levy_fraction_q) / 1000;
                const int64_t def_men  = raise_manpower(tgt, def_want);
                std::vector<army_stack_entry> def =
                    build_stack(def_men, tgt, dq ? *dq : q, def_ready);

                const battle_outcome bo = resolve_battle(
                    atk, doctrine_for(q),
                    def, dq ? doctrine_for(*dq) : doctrine_for(q),
                    comp_at(terrain, tgt.anchor), lf_at(terrain, tgt.anchor),
                    best_winter ? season::winter : season::summer,
                    atk_supply, def_supply);

                ++out.battles;
                if (century < out.battles_per_century.size())
                    ++out.battles_per_century[century];
                if (best_winter) ++out.winter_campaigns;

                // Losses are per-mille of each side's OWN committed count —
                // resolve_battle never mutates a stack, so spending them here
                // is this caller's job (combat.hpp § battle_outcome).
                const int64_t atk_lost = (stack_size(atk) * bo.attacker_losses_permille) / 1000;
                const int64_t def_lost = (stack_size(def) * bo.defender_losses_permille) / 1000;
                home.population = clampi64(home.population - atk_lost / 4, 0, 1LL << 40);
                tgt.population  = clampi64(tgt.population  - def_lost / 4, 0, 1LL << 40);

                war_pressure[static_cast<std::size_t>(src)] = clampi(bo.decisiveness / 2, 0, 1000);
                war_pressure[ti] = clampi(bo.decisiveness, 0, 1000);
                tgt.contest_q = clampi(tgt.contest_q + bo.decisiveness / 4, 0, 1000);

                // TERRITORY MOVES AT PROVINCE GRANULARITY, NEVER TILE.
                //
                // A ground-down frontier eventually gives (BL-308): the bar a
                // victory must clear falls as the region's accumulated
                // contest rises. A flat threshold meant centuries of fighting
                // over the same ground moved nothing — battles outnumbered
                // conquests by up to 1000:1 in the first sweep.
                const int relief = (tgt.contest_q * params.contest_transfer_relief_q) / 1000;
                const int needed = clampi(params.transfer_decisiveness_q - relief, 40, 1000);

                if (bo.result == battle_result::attacker_victory
                 && bo.decisiveness >= needed)
                {
                    // The loser's cohesion falls: this is where defeat starts
                    // to compound rather than merely accumulate.
                    if (dq)
                    {
                        polity& loser = out.polities[static_cast<std::size_t>(dq->id)];
                        loser.cohesion_q = clampi(loser.cohesion_q - params.cohesion_loss_on_defeat_q,
                                                  params.cohesion_floor_q, 1000);
                    }

                    // The sack — the collapse path the first sweep had none of.
                    tgt.population = clampi64(
                        tgt.population - (tgt.population * params.sack_population_loss_q) / 1000,
                        0, 1LL << 40);

                    owner[ti]  = q.id;
                    tgt.nation = q.id;
                    tgt.culture = q.culture;      // The conqueror's gods arrive...
                    tgt.creed_conquered = true;   // ...but founding_culture is never overwritten.
                    ++out.conquests;
                    out.owner_changes.push_back(owner_change{
                        static_cast<int32_t>(y), static_cast<uint16_t>(ti),
                        static_cast<uint16_t>(q.id)});
                    out.history.push_back(history_event{
                        years_from_calendar_year(y), chain_stage::legacy,
                        tgt.name + " changes hands", std::string{}});
                }
                break;
            }
            case sim_verb::settle:
            {
                const region& src = ss.regions[static_cast<std::size_t>(best_target)];

                // BL-310: find an UNOCCUPIED cell, widening the ring as the
                // neighbourhood fills. The first cut offered nine candidate
                // cells with no occupancy test and re-picked the same mature
                // parent every year, so regions piled up — 132 regions on
                // 33 distinct cells in one measured run, worst stack nine deep.
                // Co-located regions then had region_distance 0 and the
                // Ages map drew a whole stack as one dot.
                int nc = -1, nr = -1;
                for (int ring = 1; ring <= 6 && nc < 0; ++ring)
                {
                    const int span = 2 * ring + 1;
                    for (int probe = 0; probe < span * span; ++probe)
                    {
                        const uint32_t h = salt(qs, static_cast<uint32_t>(y) * 131u
                                                    + static_cast<uint32_t>(probe));
                        const int dc = static_cast<int>(h % static_cast<uint32_t>(span)) - ring;
                        const int dr = static_cast<int>((h >> 8) % static_cast<uint32_t>(span)) - ring;
                        if (dc == 0 && dr == 0) continue;

                        int cc = src.col + dc;
                        if (gw > 0) cc = ((cc % gw) + gw) % gw;
                        const int rr = clampi(src.row + dr, 0, gh > 0 ? gh - 1 : 0);

                        bool taken = false;
                        for (const region& e : ss.regions)
                            if (e.col == cc && e.row == rr) { taken = true; break; }
                        if (!taken) { nc = cc; nr = rr; break; }
                    }
                }
                if (nc < 0) break; // Neighbourhood full — no room to expand here.

                region np;
                np.col = nc;
                np.row = nr;
                np.anchor = (gw > 0) ? np.row * gw + np.col : -1;

                // Daughter ground is a decayed inheritance of the parent's —
                // good land begets good land, but never better than its parent.
                np.farm_q = (src.farm_q * 850) / 1000;
                np.ore_q  = (src.ore_q  * 700) / 1000;
                np.port_q = (src.port_q * 700) / 1000;
                np.energy_q = (src.energy_q * 700) / 1000;
                np.settle_score_q = (src.settle_score_q * 800) / 1000;
                np.culture = q.culture;
                np.founding_culture = q.culture;
                np.name = src.name + " Reach";
                np.founded_year = y;
                np.last_demography_year = y;
                np.nation = q.id;
                np.population = clampi64(region_carrying_capacity(np.farm_q) / 16, 1, 1 << 30);
                replenish_manpower(np);

                // The change list indexes regions as uint16_t, so refuse to
                // create one the time-lapse could not address (BL-312). Past
                // 65,535 the cast wrapped silently and owner_slice_at's bounds
                // check could not catch it — the wrapped index is small and in
                // range, so replay produced a plausible but WRONG map.
                if (ss.regions.size() >= owner_index_limit) break;

                ss.regions.push_back(np);
                owner.push_back(q.id);
                war_pressure.push_back(0);
                neighbours.emplace_back();
                link_region(ss.regions.size() - 1); // Keep the index complete.
                out.owner_changes.push_back(owner_change{
                    static_cast<int32_t>(y),
                    static_cast<uint16_t>(ss.regions.size() - 1),
                    static_cast<uint16_t>(q.id)});
                ++out.foundings;
                out.history.push_back(history_event{
                    years_from_calendar_year(y), chain_stage::legacy,
                    np.name + " is settled", std::string{}});
                break;
            }
            case sim_verb::invest:
            {
                const int d = clampi(best_target, 0, sim_domain_count - 1);
                // A RATE: investment accrues per year, so a coarse band must
                // credit the whole interval. Without this a 100-year band would
                // advance tech exactly as far as a 1-year one and the ladder
                // would never leave band 1 (history_sim.hpp § stepped clock).
                //
                // THE INDUSTRIAL WORKS LAND HERE (BL-321), and this is a
                // deliberate divergence from the item's own words, recorded
                // rather than hidden. The design says `industrial_mod` is a
                // "pull-forward on the Stage 4 furnace date" — but Stage 4 runs
                // inside `run_settlement`, which has already finished before
                // this loop starts, so there is no furnace date left to pull.
                // What the sim actually has as its industrial clock is the
                // capacity ladder, and a Blast Works accelerating a polity's
                // progress up that ladder is the same claim expressed against
                // the mechanism that exists. Wiring it to Stage 4 instead would
                // mean either running settlement twice or leaving the field
                // inert; this reads as the honest third option.
                const int ind_boost = clampi(mean_industrial_q, 0, 1000);
                const int progress  = clampi(best_score, 0, 1000) * step_years;
                q.progress_q[d] += progress + (progress * ind_boost) / 1000;
                // A band costs more the higher it sits — capacity follows the
                // map, and it never runs away (ANCIENT_TECH_LADDER § diffusion).
                const int cost = 4000 * q.capacity[d];
                if (q.progress_q[d] >= cost && q.capacity[d] < 6)
                {
                    q.progress_q[d] -= cost;
                    ++q.capacity[d];
                }
                break;
            }
            case sim_verb::build_work:
            {
                if (works == nullptr || best_target < 0
                 || best_target >= static_cast<int>(ss.regions.size())) break;

                region& bp = ss.regions[static_cast<std::size_t>(best_target)];
                const work_row* r = works->row_at(static_cast<std::size_t>(best_work_row));
                if (r == nullptr) break;
                if (!apply_work_to_region(bp, *works, best_work_row)) break;

                // The manpower ceiling just moved, so the stock's headroom did
                // too. Without this the Arsenal a polity built this round would
                // not be worth anything until the next demography step happened
                // to top the stock up — a work with a visible delay nobody
                // asked for.
                replenish_manpower(bp);

                ++out.works_raised;
                out.history.push_back(history_event{
                    years_from_calendar_year(y), chain_stage::legacy,
                    bp.name + " raises a " + r->name, std::string{}});
                break;
            }
            case sim_verb::consolidate:
            default:
                for (int hi : held)
                {
                    region& p = ss.regions[static_cast<std::size_t>(hi)];
                    // A RATE: a frontier cools by the year, not by the round.
                    p.contest_q = clampi(p.contest_q - 8 * step_years, 0, 1000);
                }
                // Cohesion recovers only here, and slower than it is lost — so
                // the spiral is escapable, but only by a polity that stops
                // fighting for several years running (BL-308).
                //
                // A RATE, for the same reason: `cohesion_recovery_q` is
                // documented per year of Consolidate, so a coarse band credits
                // the interval it actually covers. The clamp still bounds it,
                // so a 100-year band recovers fully rather than overshooting.
                q.cohesion_q = clampi(q.cohesion_q + params.cohesion_recovery_q * step_years,
                                      params.cohesion_floor_q, 1000);
                break;
            }
        }

        // Ownership changes are appended where they happen (conquest, founding),
        // so there is nothing to snapshot at the end of a year.
    }

    out.region_stride = static_cast<int>(ss.regions.size());
    out.years           = years;
    out.start_year      = params.start_year;

    for (polity& q : out.polities)
    {
        bool any = false;
        for (int o : owner) if (o == q.id) { any = true; break; }
        q.alive = any;
    }

    return out;
}
