// recruits.cpp — BL-1463 (MILITARY.md § Recruits, Ben 2026-10-30). See recruits.hpp.

#include "recruits.hpp"

#include "air_combat.hpp"      // air_roster_table
#include "components.hpp"
#include "logistic_points.hpp" // lp_tick::supply_share
#include "recipe_registry.hpp"
#include "unit_roster.hpp"     // hire_batch_manpower
#include "world.hpp"

#include <algorithm>
#include <cstdio>
#include <map>

namespace {

/// A base of theatre @p t that makes recruits this quarter: complete, not idled by its
/// owner, not idled by the state's funding plan.
bool base_makes(const building_component& b, recruit_theatre t)
{
    const building_type want =
        t == recruit_theatre::land ? building_type::military_base : building_type::airbase;
    return b.type == want && b.ticks_remaining <= 0 && !b.decommissioned && !b.state_idle;
}

int rate_of(const recipe_registry& reg, recruit_theatre t)
{
    const recruit_params& rp = reg.military().recruits;
    return std::max(0, t == recruit_theatre::land ? rp.land_per_base : rp.air_per_base);
}

int32_t* pool_of(nation_component& n, recruit_theatre t)
{
    return t == recruit_theatre::land ? &n.land_recruits : &n.air_recruits;
}

/// @p nation's running bases of theatre @p t, ascending building id.
std::vector<entity_id> bases_of(const world& w, const nation_component& n, recruit_theatre t)
{
    std::vector<entity_id> out;
    for (const entity_id bid : n.state_assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit != w.buildings.end() && base_makes(bit->second, t))
            out.push_back(bid);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

/// Take @p n heads from @p p (n <= its available heads): the carry takes them, and each
/// whole thousand comes off `population`. Returns the thousands lost.
int take_heads(population_centre_component& p, int64_t n)
{
    const int64_t carry = static_cast<int64_t>(p.recruit_heads_taken) + n;
    const int64_t lost  = carry / 1000;
    p.population        = static_cast<int>(static_cast<int64_t>(p.population) - lost);
    p.recruit_heads_taken = static_cast<int32_t>(carry % 1000);
    return static_cast<int>(lost);
}

} // namespace

int hire_unit_recruits(uint16_t /*row*/)
{
    // Every hire musters `hire_batch_manpower` men (advance_musters): its men are that.
    return hire_batch_manpower;
}

int wing_crew(const recipe_registry& reg, uint16_t row)
{
    const auto& table = air_roster_table();
    if (row >= table.size())
        return 0;
    return std::max(0, reg.military().air.cls[static_cast<std::size_t>(table[row].cls)].crew);
}

int recruit_pool(const world& w, entity_id nation, recruit_theatre t)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return 0;
    return t == recruit_theatre::land ? nit->second.land_recruits : nit->second.air_recruits;
}

int recruit_quarter_output(const world& w, const recipe_registry& reg, entity_id nation,
                           recruit_theatre t)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return 0;
    const int64_t out = static_cast<int64_t>(bases_of(w, nit->second, t).size()) * rate_of(reg, t);
    return static_cast<int>(std::min<int64_t>(out, k_recruit_pool_max));
}

int recruit_cap(const world& w, const recipe_registry& reg, entity_id nation, recruit_theatre t)
{
    // Clamped to the pool ceiling the save reader admits (k_recruit_pool_max).
    return static_cast<int>(std::min<int64_t>(
        static_cast<int64_t>(k_recruit_cap_quarters) * recruit_quarter_output(w, reg, nation, t),
        k_recruit_pool_max));
}

int64_t centre_heads_available(const population_centre_component& p)
{
    if (p.razed || p.population <= 0)
        return 0;
    return std::max<int64_t>(0, static_cast<int64_t>(p.population) * 1000 - p.recruit_heads_taken);
}

std::string recruit_shortfall_text(int need, int held)
{
    char buf[96];
    std::snprintf(buf, sizeof buf, "Needs %d recruits; the pool holds %d", need, held);
    return buf;
}

recruitment_tick run_recruitment(world& w, const recipe_registry& reg)
{
    recruitment_tick out;
    if (w.nations.empty() || w.buildings.empty())
        return out;

    std::vector<entity_id> nids;
    nids.reserve(w.nations.size());
    for (const auto& [nid, nc] : w.nations)
        nids.push_back(nid);
    std::sort(nids.begin(), nids.end());

    // province -> its population centres, ascending centre id. Built lazily, only for a
    // province some base stands in; `population_centre_tile` walked in ascending id.
    std::vector<entity_id> centre_ids;
    bool centres_sorted = false;
    std::map<uint32_t, std::vector<entity_id>> by_province;
    const auto centres_in = [&](uint32_t prov) -> std::vector<entity_id>& {
        if (!centres_sorted)
        {
            centre_ids.reserve(w.population_centre_tile.size());
            for (const auto& [cid, tile] : w.population_centre_tile)
                centre_ids.push_back(cid);
            std::sort(centre_ids.begin(), centre_ids.end());
            centres_sorted = true;
        }
        const auto it = by_province.find(prov);
        if (it != by_province.end())
            return it->second;
        std::vector<entity_id>& v = by_province[prov];
        if (prov != 0)
            for (const entity_id cid : centre_ids)
                if (w.population_centres.count(cid) != 0
                    && w.provinces.province_of(w.population_centre_tile.at(cid)) == prov)
                    v.push_back(cid);
        return v;
    };

    for (const entity_id nid : nids)
    {
        nation_component& nc = w.nations.at(nid);
        recruitment_row row;
        row.nation = nid;
        bool any_base = false;
        for (const recruit_theatre t : {recruit_theatre::land, recruit_theatre::air})
        {
            const std::vector<entity_id> bases = bases_of(w, nc, t);
            if (bases.empty())
                continue;
            any_base = true;
            const int rate = rate_of(reg, t);
            const int64_t cap = std::min<int64_t>(static_cast<int64_t>(k_recruit_cap_quarters)
                                                      * static_cast<int64_t>(bases.size()) * rate,
                                                  k_recruit_pool_max);
            int32_t* pool = pool_of(nc, t);
            int made = 0, shortfall = 0;
            for (const entity_id bid : bases)
            {
                // A FULL POOL TAKES NO MORE: the base makes what room is left, at most its rate.
                const int64_t room = std::max<int64_t>(0, cap - *pool);
                int want = static_cast<int>(std::min<int64_t>(rate, room));
                if (want <= 0)
                    continue;
                const uint32_t prov = w.provinces.province_of(w.buildings.at(bid).tile);
                std::vector<entity_id>& cs = centres_in(prov);
                int got = 0;
                while (got < want)
                {
                    // The centre with the most heads left, ties to the lowest id (cs is
                    // ascending, and only a strictly larger count displaces the pick).
                    entity_id pick = null_entity;
                    int64_t   best = 0;
                    for (const entity_id cid : cs)
                    {
                        const int64_t h = centre_heads_available(w.population_centres.at(cid));
                        if (h > best)
                        {
                            best = h;
                            pick = cid;
                        }
                    }
                    if (pick == null_entity)
                        break; // the province can give no more
                    const int64_t take = std::min<int64_t>(best, want - got);
                    out.centres_lost_thousand += take_heads(w.population_centres.at(pick), take);
                    out.heads_taken += take;
                    got += static_cast<int>(take);
                }
                shortfall += want - got;
                made += got;
                *pool += got;
            }
            (t == recruit_theatre::land ? row.land_made : row.air_made)   = made;
            (t == recruit_theatre::land ? row.land_short : row.air_short) = shortfall;
        }
        if (any_base)
            out.nations.push_back(row);
    }
    return out;
}

std::vector<recruit_refill> plan_recruit_refills(const world& w)
{
    std::vector<recruit_refill> plan;
    if (w.units.empty())
        return plan;
    std::vector<entity_id> ids;
    ids.reserve(w.units.size());
    for (const auto& [uid, u] : w.units)
        ids.push_back(uid);
    std::sort(ids.begin(), ids.end());
    std::map<entity_id, int64_t> left; // nation -> land pool not yet planned
    for (const entity_id uid : ids)
    {
        const unit_component& u = w.units.at(uid);
        if (u.count <= 0)
            continue;
        const int lacks = unit_establishment(u) - u.count;
        if (lacks <= 0)
            continue;
        const auto nit = w.nations.find(u.owner);
        if (nit == w.nations.end())
            continue;
        auto lit = left.find(u.owner);
        if (lit == left.end())
            lit = left.emplace(u.owner, std::max<int32_t>(0, nit->second.land_recruits)).first;
        const int planned = static_cast<int>(std::min<int64_t>(lacks, lit->second));
        if (planned <= 0)
            continue;
        lit->second -= planned;
        plan.push_back({uid, u.owner, planned});
    }
    return plan;
}

float recruit_refill_lp_need(const recipe_registry& reg, const recruit_refill& r)
{
    return static_cast<float>(r.planned) * reg.military().recruits.lp_per_recruit;
}

int apply_recruit_refills(world& w, const std::vector<recruit_refill>& plan, const lp_tick& lp)
{
    int delivered = 0;
    for (const recruit_refill& r : plan) // ascending unit id
    {
        const auto uit = w.units.find(r.unit);
        const auto nit = w.nations.find(r.nation);
        if (uit == w.units.end() || nit == w.nations.end() || uit->second.count <= 0)
            continue;
        // The resolver granted the refill in WHOLE recruits and drew points only for
        // those (`lp_supply_row::recruits_delivered`). The supplies are served in
        // ascending unit id, so a binary search finds the row.
        const auto sit = std::lower_bound(lp.supplies.begin(), lp.supplies.end(), r.unit,
            [](const lp_supply_row& s, entity_id v) { return s.unit < v; });
        if (sit == lp.supplies.end() || sit->unit != r.unit)
            continue;
        unit_component& u = uit->second;
        int n = std::min(sit->recruits_delivered, r.planned);
        // Re-bounded against the live state: nothing between the plan and here may have
        // moved either, but a refill never overfills a unit or overdraws a pool.
        n = std::min(n, unit_establishment(u) - u.count);
        n = std::min<int>(n, std::max<int32_t>(0, nit->second.land_recruits));
        if (n <= 0)
            continue;
        u.count += n;
        nit->second.land_recruits -= n;
        delivered += n;
    }
    return delivered;
}

// ---- Wings refill too (Ben, 2026-10-31) -----------------------------------------

std::vector<wing_refill> plan_wing_refills(const world& w, const recipe_registry& reg)
{
    std::vector<wing_refill> plan;
    std::map<entity_id, int64_t> left; // nation -> air pool not yet planned
    for (const auto& [wid, wg] : w.wings) // std::map: ascending wing id
    {
        if (wg.muster_days > 0 || wg.hp_ppm <= 0 || wg.hp_ppm >= k_wing_hp_full)
            continue;
        const auto nit = w.nations.find(wg.owner);
        if (nit == w.nations.end())
            continue;
        const entity_id tile = wing_base_tile(w, wg);
        if (tile == null_entity)
            continue;
        const int crew = wing_crew(reg, wg.type);
        if (crew <= 0)
            continue;
        const int64_t missing = static_cast<int64_t>(k_wing_hp_full) - wg.hp_ppm;
        const int64_t lacks   = (missing * crew + k_wing_hp_full - 1) / k_wing_hp_full; // ceil, >= 1
        auto lit = left.find(wg.owner);
        if (lit == left.end())
            lit = left.emplace(wg.owner, std::max<int32_t>(0, nit->second.air_recruits)).first;
        const int planned = static_cast<int>(std::min<int64_t>(lacks, lit->second));
        if (planned <= 0)
            continue;
        lit->second -= planned;
        plan.push_back({wid, wg.owner, tile, planned, static_cast<int>(lacks)});
    }
    return plan;
}

int apply_wing_refills(world& w, const recipe_registry& reg, const std::vector<wing_refill>& plan,
                       const lp_tick& lp)
{
    int delivered = 0;
    for (const wing_refill& r : plan) // ascending wing id
    {
        const auto wit = w.wings.find(r.wing);
        const auto nit = w.nations.find(r.nation);
        if (wit == w.wings.end() || nit == w.nations.end() || wit->second.hp_ppm <= 0)
            continue;
        const auto sit = std::lower_bound(lp.supplies.begin(), lp.supplies.end(), r.wing,
            [](const lp_supply_row& s, entity_id v) { return s.unit < v; });
        if (sit == lp.supplies.end() || sit->unit != r.wing)
            continue;
        air_wing& wg = wit->second;
        const int crew = wing_crew(reg, wg.type);
        if (crew <= 0)
            continue;
        int n = std::min(sit->recruits_delivered, r.planned);
        n = std::min<int>(n, std::max<int32_t>(0, nit->second.air_recruits));
        if (n <= 0)
            continue;
        const int64_t gain = n >= r.lacks
            ? static_cast<int64_t>(k_wing_hp_full) - wg.hp_ppm
            : static_cast<int64_t>(n) * k_wing_hp_full / crew;
        wg.hp_ppm = static_cast<int32_t>(std::min<int64_t>(k_wing_hp_full, wg.hp_ppm + gain));
        nit->second.air_recruits -= n;
        delivered += n;
    }
    return delivered;
}

// ---- A dropped muster refunds its men (Ben, 2026-10-31) ---------------------------

int refund_muster_recruits(world& w, const muster_order& m)
{
    const auto nit = w.nations.find(m.owner);
    if (nit == w.nations.end())
        return 0;
    const int n = hire_unit_recruits(m.unit_type);
    int32_t& pool = nit->second.land_recruits;
    const int64_t after = std::min<int64_t>(static_cast<int64_t>(pool) + n, k_recruit_pool_max);
    const int given = static_cast<int>(after - pool);
    pool = static_cast<int32_t>(after);
    return given;
}
