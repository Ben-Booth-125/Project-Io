#include "history_sim.hpp"

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
int jitter(int base, uint32_t s, int spread)
{
    if (spread <= 0) return base;
    const int d = static_cast<int>(salt(s, 0x51ED2701u) % static_cast<uint32_t>(2 * spread + 1)) - spread;
    return base + (base * d) / 1000;
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

/// Turn raised manpower into a typed stack. Composition follows the polity's
/// endowment and military band rather than a roster table — BL-274 owns the
/// roster proper; this is the coarse class split `resolve_battle` scores on.
///
/// `readiness_q` scales every entry's power modifier: the caller-side lever the
/// winter-campaign candidate uses against a defender (history_sim.hpp § season).
std::vector<army_stack_entry> build_stack(int64_t manpower,
                                          const province& home,
                                          const polity&   owner,
                                          int             readiness_q)
{
    std::vector<army_stack_entry> stack;
    if (manpower <= 0) return stack;

    const int band = clampi(owner.capacity[static_cast<int>(sim_domain::military)], 1, 6);

    // Class split, per-mille. Ore country fields more heavy foot; open ground
    // and a later band field more horse. Ranged and siege stay minorities.
    int inf = 560, cav = 160, rng = 200, sge = 80;
    if (home.ore_q > 600) { inf += 80; rng -= 40; cav -= 40; }
    if (band >= 2)        { cav += 60; inf -= 60; }   // The stirrup — a T2 unlock.
    if (home.port_q > 600) { sge += 30; inf -= 30; }

    const int   power_mod = ((band - 1) * 60) + ((readiness_q - 1000) / 10);
    const auto  entry = [&](unit_class c, int share, uint16_t tag) {
        army_stack_entry e;
        e.type_id  = tag;
        e.cls      = c;
        e.count    = static_cast<int>((manpower * share) / 1000);
        e.type_power_mod = power_mod;
        if (e.count > 0) stack.push_back(e);
    };

    entry(unit_class::infantry, inf, 1);
    entry(unit_class::cavalry,  cav, 2);
    entry(unit_class::ranged,   rng, 3);
    entry(unit_class::siege,    sge, 4);
    return stack;
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

    // --- Time-lapse change list -------------------------------------------
    out.owner_changes.clear();
    for (std::size_t i = 0; i < owner.size(); ++i)
        if (owner[i] >= 0)
            out.owner_changes.push_back(owner_change{
                static_cast<int32_t>(params.start_year),
                static_cast<uint16_t>(i),
                static_cast<uint16_t>(owner[i])});

    const int64_t years = params.stop_year - params.start_year;

    for (int64_t y = params.start_year; y < params.stop_year; ++y)
    {
        // ---- Demography -------------------------------------------------
        for (std::size_t i = 0; i < ss.provinces.size(); ++i)
        {
            advance_province_demography(ss.provinces[i], 1, war_pressure[i]);
            war_pressure[i] = 0;
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
                q.capital = held.front(); // Capital fell: the largest holding takes over.

            const province& cap = ss.provinces[static_cast<std::size_t>(q.capital)];
            const uint32_t  qs  = salt(seed, static_cast<uint32_t>(q.id));

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

                    int value = (jitter(params.w_farm, qs, 120) * tgt.farm_q
                              +  jitter(params.w_ore,  qs, 120) * tgt.ore_q
                              +  jitter(params.w_port, qs, 120) * tgt.port_q) / 1000;

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

                    value -= params.w_dist * cap_dist / 10;
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

                        int s = value - (params.w_def * def_eff) / 1000;
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
                if (pressure_src >= 0 && pressure_best >= params.settle_pressure_q)
                {
                    const int s = pressure_best - params.settle_pressure_q + params.settle_threshold_q;
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
                const int s = static_cast<int>(clampi64(pop / 4000, 0, 1000));
                if (s > best_score && s >= params.invest_threshold_q)
                {
                    best_score = s; best_verb = sim_verb::invest; best_target = dom;
                    best_winter = false;
                }
            }

            // -- Consolidate (the always-legal fallback) -------------------
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
                const int atk_supply = clampi(1000 - cap_dist * params.supply_decay_per_tile_q, 0, 1000);
                const int def_supply = 1000;

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
                if (best_winter) ++out.winter_campaigns;
                if (atk_supply == 0) ++out.stalled_campaigns;

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
                if (bo.result == battle_result::attacker_victory
                 && bo.decisiveness >= params.transfer_decisiveness_q)
                {
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

                province np;
                np.col = src.col + 1 + static_cast<int>(salt(qs, static_cast<uint32_t>(y)) % 3u);
                np.row = src.row + static_cast<int>(salt(qs, static_cast<uint32_t>(y) ^ 0x5Au) % 3u) - 1;
                if (gw > 0) np.col = ((np.col % gw) + gw) % gw; // Columns wrap.
                np.row = clampi(np.row, 0, gh > 0 ? gh - 1 : 0);
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
