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
/// annual gain in ENDOWMENT VALUE HELD, where a province is worth the mean of
/// its three endowment windows (0-1000).
///
/// This replaces comparing four incommensurable numbers. The first cut compared
/// raw scores directly, and Invest — whose score was population/4000 clamped —
/// pinned at its ceiling and won forever. The second cut normalised each verb
/// by its own range, which structurally favours the NARROWEST range and simply
/// handed the win to Settle instead. Neither was fixable by tuning, because the
/// error was upstream of the constants: quantities that mean different things
/// cannot be ranked. So each verb now answers the same question — what is this
/// worth to me this year, in provinces-worth-of-endowment — and the argmax is
/// an honest comparison rather than a coincidence of scales.
int province_value_q(const province& p)
{
    return (p.farm_q + p.ore_q + p.port_q) / 3;
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
                                          const province& home,
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
    std::vector<uint16_t> slice(static_cast<std::size_t>(s.province_stride), owner_none);
    for (const owner_change& c : s.owner_changes)
    {
        if (static_cast<int64_t>(c.year) > year) break; // Appended in year order.
        if (c.province < slice.size()) slice[c.province] = c.owner;
    }
    return slice;
}

int province_distance(const province& a, const province& b, int gw)
{
    int dc = a.col - b.col;
    if (dc < 0) dc = -dc;
    if (gw > 0 && dc > gw / 2) dc = gw - dc; // Columns wrap: the map is a cylinder.
    int dr = a.row - b.row;
    if (dr < 0) dr = -dr;
    return dc > dr ? dc : dr; // Chebyshev — movement is eight-connected.
}

// ---------------------------------------------------------------------------

history_sim_state run_history_sim(settlement_state&         ss,
                                  const creed_state*        cs,
                                  const sim_terrain_view&   terrain,
                                  int                       gw,
                                  int                       gh,
                                  const history_sim_params& params,
                                  uint32_t                  seed)
{
    history_sim_state out;
    if (ss.provinces.empty() || params.stop_year <= params.start_year)
        return out;

    // --- Seed polities from cultures --------------------------------------
    //
    // At the antiquity start `province::nation` is -1: the political pass has
    // not run, and a pre-national world's actors ARE its peoples (BL-221). The
    // sim writes `nation` as it goes, so the political map is this loop's
    // output rather than its input.
    {
        std::vector<int> cultures;
        for (const province& p : ss.provinces)
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

            // Capital: the best-settled province of this culture. Ties break on
            // the lower index, which is placement order (best ground first).
            int best = -1, best_q = -1;
            for (std::size_t i = 0; i < ss.provinces.size(); ++i)
            {
                const province& p = ss.provinces[i];
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
            for (const province& p : ss.provinces) if (p.culture == q.culture) ++n;
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

    // Province -> owning polity. Seeded from culture, then owned by conquest.
    std::vector<int> owner(ss.provinces.size(), -1);
    for (std::size_t i = 0; i < ss.provinces.size(); ++i)
        for (const polity& q : out.polities)
            if (q.culture == ss.provinces[i].culture) { owner[i] = q.id; break; }

    for (std::size_t i = 0; i < ss.provinces.size(); ++i)
    {
        province& p = ss.provinces[i];
        p.nation = owner[i];
        // Seed a headcount so demography has something to grow from — the
        // graduation path settlement.hpp's demography note leaves to this item.
        if (p.population <= 0)
            p.population = clampi64(province_carrying_capacity(p.farm_q) / 8, 1, 1 << 30);
        p.last_demography_year = params.start_year;
        replenish_manpower(p);
    }

    // --- Per-year war pressure, reset each tick ---------------------------
    std::vector<int> war_pressure(ss.provinces.size(), 0);

    // --- Neighbour index --------------------------------------------------
    //
    // Campaign candidates are NEIGHBOURS ONLY, so the neighbourhood is built
    // once rather than rediscovered by scanning every province from every held
    // province every year. That scan is quadratic in province count and, with
    // the Settle verb growing the map past 400 provinces, it dominated the
    // whole run (2.5s of a 2.5s run). Built once here, extended when a
    // province is founded, it is a lookup.
    std::vector<std::vector<int>> neighbours(ss.provinces.size());
    const auto link_province = [&](std::size_t i) {
        for (std::size_t j = 0; j < ss.provinces.size(); ++j)
        {
            if (i == j) continue;
            if (province_distance(ss.provinces[i], ss.provinces[j], gw) <= params.neighbour_radius)
            {
                neighbours[i].push_back(static_cast<int>(j));
                neighbours[j].push_back(static_cast<int>(i));
            }
        }
    };
    for (std::size_t i = 0; i < ss.provinces.size(); ++i)
        for (std::size_t j = i + 1; j < ss.provinces.size(); ++j)
            if (province_distance(ss.provinces[i], ss.provinces[j], gw) <= params.neighbour_radius)
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
    std::vector<int> reach;            // Per-province cost from the current capital.
    int reach_capital = -2;            // Which capital `reach` was built for.

    const auto tile_cost = [&](const province& p) {
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
        reach.assign(ss.provinces.size(), 1 << 28);
        if (capital < 0 || capital >= static_cast<int>(ss.provinces.size())) return;
        reach[static_cast<std::size_t>(capital)] = 0;

        // Dijkstra without a heap: province counts are hundreds, and a simple
        // scan keeps the order deterministic without depending on a tie-break
        // inside a priority queue.
        std::vector<bool> done(ss.provinces.size(), false);
        for (std::size_t iter = 0; iter < ss.provinces.size(); ++iter)
        {
            int best = -1, best_c = 1 << 28;
            for (std::size_t i = 0; i < reach.size(); ++i)
                if (!done[i] && reach[i] < best_c) { best_c = reach[i]; best = static_cast<int>(i); }
            if (best < 0) break;
            done[static_cast<std::size_t>(best)] = true;

            const province& bp = ss.provinces[static_cast<std::size_t>(best)];
            for (int nb : neighbours[static_cast<std::size_t>(best)])
            {
                const province& np2 = ss.provinces[static_cast<std::size_t>(nb)];
                const int step = province_distance(bp, np2, gw)
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

    for (int64_t y = params.start_year; y < params.stop_year; ++y)
    {
        const std::size_t century =
            static_cast<std::size_t>((y - params.start_year) / 100);

        // ---- Demography -------------------------------------------------
        int64_t total_pop = 0;
        for (std::size_t i = 0; i < ss.provinces.size(); ++i)
        {
            advance_province_demography(ss.provinces[i], 1, war_pressure[i]);
            war_pressure[i] = 0;
            total_pop += ss.provinces[i].population;
        }
        if (total_pop > out.peak_population)
        {
            out.peak_population = total_pop;
            out.peak_year       = y;
        }

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
                // surviving province — placement order, which is best-ground
                // first, so it is a reasonable seat without being "the largest
                // holding" the first cut's comment claimed (BL-312).
                q.capital = held.front();

            const province& cap = ss.provinces[static_cast<std::size_t>(q.capital)];
            const uint32_t  qs  = salt(seed, static_cast<uint32_t>(q.id));

            if (reach_capital != q.capital || reach.size() != ss.provinces.size())
                rebuild_reach(q.capital);

            // THE BURDEN OF BREADTH (BL-314 S3). Every province held past
            // `free_holdings` costs supply on every campaign this polity runs.
            const int over = static_cast<int>(held.size()) - params.free_holdings;
            const int burden = over > 0 ? over * params.holdings_burden_q : 0;

            // ---- Build the bounded candidate set -------------------------
            //
            // Four verbs. The set is bounded by construction: neighbours only,
            // one settle site, one invest domain, one consolidate.
            sim_verb best_verb  = sim_verb::none;
            int      best_score = 0;
            int      best_target = -1;
            bool     best_winter = false;

            // -- Campaign --------------------------------------------------
            for (int hi : held)
            {
                for (int tn : neighbours[static_cast<std::size_t>(hi)])
                {
                    const std::size_t ti = static_cast<std::size_t>(tn);
                    const int to = owner[ti];
                    if (to == q.id || to < 0) continue;

                    const province& tgt = ss.provinces[ti];
                    const int cap_dist = province_distance(cap, tgt, gw);

                    // Defender's fielded power, as a headcount proxy.
                    const int64_t def_men = (tgt.manpower_stock * params.levy_fraction_q) / 1000;
                    const int def_scaled  = static_cast<int>(clampi64(def_men / 64, 0, 1000));

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
                            if (ss.provinces[static_cast<std::size_t>(h2)].port_q > 400
                             && province_distance(ss.provinces[static_cast<std::size_t>(h2)], tgt, gw)
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
                    const int reach_here = (ti < reach.size() && reach[ti] < (1 << 27))
                                         ? static_cast<int>(reach[ti]) : cap_dist;
                    const int supply_here = clampi(1000
                                          - cap_dist * params.supply_decay_per_tile_q
                                          - reach_here * params.terrain_reach_cost_q / 100
                                          - clampi(burden, 0, 1000 - params.holdings_burden_floor_q),
                                          0, 1000);

                    int64_t atk_men = 0;
                    for (int hi2 : held)
                        if (ss.provinces[static_cast<std::size_t>(hi2)].manpower_stock > atk_men)
                            atk_men = ss.provinces[static_cast<std::size_t>(hi2)].manpower_stock;
                    const int64_t atk_est = (atk_men * supply_here / 1000)
                                          * clampi(q.cohesion_q, 1, 1000) / 1000;
                    const int64_t def_est = tgt.manpower_stock > 0 ? tgt.manpower_stock : 1;
                    const int p_win_q = static_cast<int>(
                        clampi64((atk_est * 1000) / (atk_est + def_est), 0, 1000));

                    value = (value * p_win_q) / 1000;

                    // Costs, in the same unit: distance is a real logistics
                    // cost now (BL-314), not just a preference.
                    value -= params.w_dist * cap_dist / 10;
                    value -= (1000 - supply_here) * params.campaign_supply_cost_q / 1000;
                    if (tgt.culture != q.culture) value -= params.w_cult;

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
            // province before the stop year and leaves none after it, so
            // without this verb a 2000-year run has a frozen province count.
            {
                int pressure_best = -1, pressure_src = -1;
                for (int hi : held)
                {
                    const province& p = ss.provinces[static_cast<std::size_t>(hi)];
                    const int64_t K = province_carrying_capacity(p.farm_q);
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
                    const province& sp = ss.provinces[static_cast<std::size_t>(pressure_src)];
                    const int daughter_value = (province_value_q(sp) * 800) / 1000;
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
                for (int hi : held) pop += ss.provinces[static_cast<std::size_t>(hi)].population;

                // Divided by the band already held: each further band costs more
                // to want, so investment does not pin at its ceiling the moment
                // a polity has a few mature provinces (BL-309). Marginal value,
                // not accumulated size.
                int64_t holdings_value = 0;
                for (int hi : held)
                    holdings_value += province_value_q(ss.provinces[static_cast<std::size_t>(hi)]);
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
                    hv += province_value_q(ss.provinces[static_cast<std::size_t>(hi)]);
                const int shortfall = 1000 - clampi(q.cohesion_q, 0, 1000);
                const int s = static_cast<int>(clampi64(
                    (hv * shortfall) / (1000LL * params.consolidate_divisor), 0, 1000));
                if (s > best_score && s >= params.consolidate_threshold_q)
                {
                    best_score = s; best_verb = sim_verb::consolidate;
                    best_target = -1; best_winter = false;
                }
            }
            if (best_verb == sim_verb::none) best_verb = sim_verb::consolidate;

            // ---- Execute -------------------------------------------------
            switch (best_verb)
            {
            case sim_verb::campaign:
            {
                const std::size_t ti = static_cast<std::size_t>(best_target);
                province& tgt = ss.provinces[ti];

                // Nearest holding is the staging province.
                int src = held.front(), src_d = 1 << 30;
                for (int hi : held)
                {
                    const int d = province_distance(ss.provinces[static_cast<std::size_t>(hi)], tgt, gw);
                    if (d < src_d) { src_d = d; src = hi; }
                }
                province& home = ss.provinces[static_cast<std::size_t>(src)];

                const int64_t want   = (home.manpower_stock * params.levy_fraction_q) / 1000;
                const int64_t raised = raise_manpower(home, want);
                if (raised <= 0) break;

                // FORCE COMMITMENT (BL-277 Q2): supply decays with distance
                // from the CAPITAL, not the staging province — an empire
                // fighting at its rim is supplied from its centre. This is the
                // stall: far enough out, the arriving force is under the
                // defender's and the frontier stops on arithmetic alone.
                const int cap_dist = province_distance(cap, tgt, gw);

                // Terrain-weighted reach where it is known, straight-line
                // otherwise (a target outside the connected component).
                const int reach_cost = (static_cast<std::size_t>(best_target) < reach.size()
                                        && reach[static_cast<std::size_t>(best_target)] < (1 << 27))
                                     ? reach[static_cast<std::size_t>(best_target)] : cap_dist;

                const int supply_raw = 1000
                                     - cap_dist * params.supply_decay_per_tile_q
                                     - reach_cost * params.terrain_reach_cost_q / 100
                                     - clampi(burden, 0, 1000 - params.holdings_burden_floor_q);
                const int atk_supply = clampi(supply_raw, 0, 1000);
                const int def_supply = 1000;

                // The stall, counted where it actually happens (BL-312). The
                // first cut incremented this AFTER resolve_battle and only at
                // exactly zero supply, so it counted launched battles rather
                // than stalls and measured a constant zero in every run.
                if (atk_supply < params.stalled_supply_q) ++out.stalled_campaigns;

                const int def_ready = best_winter ? (1000 - params.winter_readiness_penalty_q) : 1000;

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
                // victory must clear falls as the province's accumulated
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
                const province& src = ss.provinces[static_cast<std::size_t>(best_target)];

                // BL-310: find an UNOCCUPIED cell, widening the ring as the
                // neighbourhood fills. The first cut offered nine candidate
                // cells with no occupancy test and re-picked the same mature
                // parent every year, so provinces piled up — 132 provinces on
                // 33 distinct cells in one measured run, worst stack nine deep.
                // Co-located provinces then had province_distance 0 and the
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
                        for (const province& e : ss.provinces)
                            if (e.col == cc && e.row == rr) { taken = true; break; }
                        if (!taken) { nc = cc; nr = rr; break; }
                    }
                }
                if (nc < 0) break; // Neighbourhood full — no room to expand here.

                province np;
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
                np.population = clampi64(province_carrying_capacity(np.farm_q) / 16, 1, 1 << 30);
                replenish_manpower(np);

                // The change list indexes provinces as uint16_t, so refuse to
                // create one the time-lapse could not address (BL-312). Past
                // 65,535 the cast wrapped silently and owner_slice_at's bounds
                // check could not catch it — the wrapped index is small and in
                // range, so replay produced a plausible but WRONG map.
                if (ss.provinces.size() >= owner_index_limit) break;

                ss.provinces.push_back(np);
                owner.push_back(q.id);
                war_pressure.push_back(0);
                neighbours.emplace_back();
                link_province(ss.provinces.size() - 1); // Keep the index complete.
                out.owner_changes.push_back(owner_change{
                    static_cast<int32_t>(y),
                    static_cast<uint16_t>(ss.provinces.size() - 1),
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
                q.progress_q[d] += clampi(best_score, 0, 1000);
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
            case sim_verb::consolidate:
            default:
                for (int hi : held)
                {
                    province& p = ss.provinces[static_cast<std::size_t>(hi)];
                    p.contest_q = clampi(p.contest_q - 8, 0, 1000);
                }
                // Cohesion recovers only here, and slower than it is lost — so
                // the spiral is escapable, but only by a polity that stops
                // fighting for several years running (BL-308).
                q.cohesion_q = clampi(q.cohesion_q + params.cohesion_recovery_q,
                                      params.cohesion_floor_q, 1000);
                break;
            }
        }

        // Ownership changes are appended where they happen (conquest, founding),
        // so there is nothing to snapshot at the end of a year.
    }

    out.province_stride = static_cast<int>(ss.provinces.size());
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
