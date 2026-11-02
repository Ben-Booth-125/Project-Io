// recruits.cpp — BL-1463 (MILITARY.md § Recruits, Ben 2026-10-30). See recruits.hpp.

#include "recruits.hpp"
#include "economy_system.hpp" // BL-1423: unit_supply_need - the supply bill a refill plans after

#include "air_combat.hpp"      // air_roster_table
#include "components.hpp"
#include "logistic_points.hpp" // lp_tick::supply_share
#include "recipe_registry.hpp"
#include "unit_designs.hpp"    // BL-1423: kit_goods, unit_land_class - a refill's equipment
#include "unit_roster.hpp"     // hire_batch_manpower
#include "world.hpp"

#include <algorithm>

#include <cmath>
#include <limits>
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

std::array<float, resource_count> refill_goods_per_man(const world& w, const unit_component& u)
{
    std::array<float, resource_count> per{};
    const std::vector<roster_row>& table = unit_roster_table();
    if (u.type >= table.size())
        return per;
    const roster_row& row = table[u.type];
    // What a hire of this unit buys: its recorded design's kit goods (none for base kit)
    // and each gated axis's flat cost at the candidate a HIRE would take - the first one
    // the owner's shelves hold `hire_axis_cost` of beyond what the kit and the earlier
    // axes already draw (corp_command.cpp `plan_hire_goods`, the same rule), so a unit
    // its nation could hire it can refill. No candidate covering: the first (and the
    // shelf then cannot equip the man, so he is not planned). The unit's OWN kit, so a
    // stale design refills on the kit it fights with.
    per = kit_goods(unit_land_class(u), u.kit);
    for (std::size_t i = 0; i < hire_axis_count; ++i)
    {
        const hire_axis axis = static_cast<hire_axis>(i);
        if (gate_axis_q(row.gate, axis) <= 0)
            continue;
        resource_type pick = hire_axis_resources(axis).candidates[0];
        for (const resource_type r : hire_axis_resources(axis))
            if (corp_stockpile_total(w, u.owner, r) >= per[static_cast<std::size_t>(r)] + hire_axis_cost)
            {
                pick = r;
                break;
            }
        per[static_cast<std::size_t>(pick)] += hire_axis_cost;
    }
    for (float& q : per)
        q /= static_cast<float>(hire_batch_manpower);
    return per;
}

namespace {

/// Float noise on a whole man's share: a shelf holding n men's goods to within this
/// fraction of ONE man's share equips n. Never more than 1e-5 of a man over-granted, and
/// `apply_recruit_refills` clamps to what was actually bought with the same tolerance.
constexpr float k_refit_share_eps = 1e-5f;

/// The men @p avail goods equip at @p per a man (INT_MAX when nothing is needed).
int men_equipped(const std::array<float, resource_count>& avail,
                 const std::array<float, resource_count>& per)
{
    int64_t n = std::numeric_limits<int32_t>::max();
    for (std::size_t r = 0; r < resource_count; ++r) // resource order: a fixed walk
        if (per[r] > 0.0f)
            n = std::min<int64_t>(n, static_cast<int64_t>(std::floor(std::max(0.0f, avail[r]) / per[r]
                                                                     + k_refit_share_eps)));
    return static_cast<int>(std::max<int64_t>(0, n));
}

/// The highest posted price @p nation could pay for @p r on its shelves.
float shelf_price_bound(const world& w, entity_id nation, std::size_t r)
{
    float px = 0.0f;
    for (const entity_id mid : corp_markets(w, nation))
        if (const auto mit = w.markets.find(mid); mit != w.markets.end())
            px = std::max(px, posted_price(mit->second, r));
    return px;
}

/// THE PLAN'S SHELVES (BL-1423 cold review): every market's buyable stock, read once and
/// RESERVED as units plan, across ALL nations - two nations on one market draw on one
/// shelf - market by market in each nation's own `corp_markets` order, exactly as
/// `buy_from_corp_shelves` takes it. A market is read on first use: priced and admitted
/// to an off-buy (`shelf_admits`), else it holds nothing for the refill.
struct shelf_book
{
    std::map<entity_id, std::array<float, resource_count>> left;

    std::array<float, resource_count>& of(const world& w, entity_id mid)
    {
        auto it = left.find(mid);
        if (it != left.end())
            return it->second;
        std::array<float, resource_count> s{};
        const market_component& mc = w.markets.at(mid);
        for (std::size_t r = 0; r < resource_count; ++r)
            if (mc.base_price[r] > 0.0f && shelf_admits(mc, r, 0.0f, /*off_buys=*/true))
                s[r] = std::max(0.0f, mc.inventory[r]);
        return left.emplace(mid, s).first->second;
    }

    /// What @p nation's markets hold of each good, unreserved.
    std::array<float, resource_count> avail(const world& w, const std::vector<entity_id>& mids)
    {
        std::array<float, resource_count> a{};
        for (const entity_id mid : mids)
        {
            const std::array<float, resource_count>& s = of(w, mid);
            for (std::size_t r = 0; r < resource_count; ++r)
                a[r] += s[r];
        }
        return a;
    }

    /// Reserve @p qty of each good, filling markets in @p mids order (the buy's order).
    void take(const world& w, const std::vector<entity_id>& mids, const std::array<float, resource_count>& qty)
    {
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            float want = qty[r];
            for (const entity_id mid : mids)
            {
                if (!(want > 0.0f))
                    break;
                float& s = of(w, mid)[r];
                const float t = std::min(want, s);
                s -= t;
                want -= t;
            }
        }
    }
};

} // namespace

std::vector<recruit_refill> plan_recruit_refills(const world& w, const recipe_registry& reg)
{
    std::vector<recruit_refill> plan;
    if (w.units.empty())
        return plan;
    std::vector<entity_id> ids;
    ids.reserve(w.units.size());
    for (const auto& [uid, u] : w.units)
        ids.push_back(uid);
    std::sort(ids.begin(), ids.end());

    // THE CREDITS THE QUARTER'S SUPPLY DRAW WILL BILL (BL-1423 cold review). The supply
    // draw (`charge_supply_draws`) bills active_lp_credit_per_unit_distance x the points
    // granted from the same treasury BEFORE the refill buys its kit, so the room a refill
    // may plan against is the room LESS that bill at its upper bound: every unit's whole
    // supply request (granted <= need), and each planned recruit's points as planned.
    const military_capability_params& mil = reg.military();
    const float rate = mil.active_lp_credit_per_unit_distance > 0.0f ? mil.active_lp_credit_per_unit_distance : 0.0f;
    std::map<entity_id, float> room; // nation -> purse room not yet planned
    for (const entity_id uid : ids)
    {
        const unit_component& u = w.units.at(uid);
        if (u.count <= 0 || w.nations.find(u.owner) == w.nations.end())
            continue;
        auto rit = room.find(u.owner);
        if (rit == room.end())
            rit = room.emplace(u.owner, std::max(0.0f, purse_room_of(w, u.owner))).first;
        rit->second -= rate * unit_supply_need(u, mil.upkeep);
    }

    std::map<entity_id, int64_t> left; // nation -> land pool not yet planned
    shelf_book shelves;
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
        int planned = static_cast<int>(std::min<int64_t>(lacks, lit->second));
        if (planned <= 0)
            continue;
        const std::array<float, resource_count> per = refill_goods_per_man(w, u);
        const std::vector<entity_id> mids = corp_markets(w, u.owner);
        planned = std::min(planned, men_equipped(shelves.avail(w, mids), per));
        // And the purse room: each man's kit at the highest posted price plus his
        // points' credit bill, after the units before it and the supply bill above.
        float kit_per_man = 0.0f, per_cost = 0.0f;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (per[r] > 0.0f)
            {
                kit_per_man += per[r];
                per_cost    += per[r] * shelf_price_bound(w, u.owner, r);
            }
        per_cost += rate * (mil.recruits.lp_per_recruit + kit_per_man);
        float& rm = room.at(u.owner);
        if (per_cost > 0.0f)
            planned = std::min<int>(planned, static_cast<int>(std::floor(std::max(0.0f, rm) / per_cost)));
        if (planned <= 0)
            continue;
        rm -= per_cost * static_cast<float>(planned);
        lit->second -= planned;
        std::array<float, resource_count> qty{};
        for (std::size_t r = 0; r < resource_count; ++r)
            qty[r] = per[r] * static_cast<float>(planned);
        shelves.take(w, mids, qty);
        plan.push_back({uid, u.owner, planned});
    }
    return plan;
}

float recruit_refill_lp_need(const recipe_registry& reg, const recruit_refill& r)
{
    return static_cast<float>(r.planned) * reg.military().recruits.lp_per_recruit;
}

int apply_recruit_refills(world& w, const std::vector<recruit_refill>& plan, const lp_tick& lp,
                          refit_tally* tally)
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
        // BL-1423: EACH MAN COMES WITH HIS EQUIPMENT. The plan reserved his kit on the
        // shelves (across every nation, in this same ascending-unit order) and his credits
        // after the supply bill, so the buy below finds both; it is re-bounded against the
        // live state anyway and then CLAMPED to what the shelves actually sold.
        const std::array<float, resource_count> per = refill_goods_per_man(w, u);
        const int granted = n;
        {
            std::array<float, resource_count> avail{};
            float per_cost = 0.0f;
            for (const entity_id mid : corp_markets(w, r.nation))
            {
                const market_component& mc = w.markets.at(mid);
                for (std::size_t g = 0; g < resource_count; ++g)
                    if (per[g] > 0.0f && mc.base_price[g] > 0.0f && shelf_admits(mc, g, 0.0f, true))
                        avail[g] += std::max(0.0f, mc.inventory[g]);
            }
            for (std::size_t g = 0; g < resource_count; ++g)
                if (per[g] > 0.0f)
                    per_cost += per[g] * shelf_price_bound(w, r.nation, g);
            n = std::min(n, men_equipped(avail, per));
            if (per_cost > 0.0f)
                n = std::min<int>(n, static_cast<int>(std::max(0.0f, std::floor(purse_room_of(w, r.nation) / per_cost))));
        }
        if (n <= 0)
        {
            if (tally != nullptr)
                tally->short_of_goods += granted;
            continue;
        }
        std::array<float, resource_count> bought{};
        for (std::size_t g = 0; g < resource_count; ++g)
            if (per[g] > 0.0f)
                bought[g] = buy_from_corp_shelves(w, r.nation, g, per[g] * static_cast<float>(n), 0.0f);
        // The men equipped are those the goods ACTUALLY bought cover (to float noise on
        // one man's share); never more than were granted.
        n = std::min(n, men_equipped(bought, per));
        if (tally != nullptr)
        {
            tally->short_of_goods += granted - n;
            for (std::size_t g = 0; g < resource_count; ++g)
                tally->goods[g] += bought[g];
        }
        if (n <= 0)
            continue;
        u.count += n;
        nit->second.land_recruits -= n;
        delivered += n;
        if (tally != nullptr)
            tally->men += n;
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
