#include "network_upkeep.hpp"

#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

namespace {

/// The lowest-id market on @p body, or null. The same deterministic pick
/// `any_market_on_body` (corp_command.cpp) makes; a file-scope copy for the
/// same reason space_programme.cpp carries one — that helper is private to
/// the command seam and this TU must stay linkable without it.
entity_id lowest_market_on_body(const world& w, entity_id body)
{
    entity_id best = null_entity;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body && (best == null_entity || mid < best))
            best = mid;
    return best;
}

/// The procurement price basis: the resolved price at @p body's market, or
/// `base_price` before first resolution — space_programme.cpp's own reading.
/// Zero when the body has no market at all: a purchase with no price basis is
/// refused, never priced at nothing — a zero-credit draw would be confiscation
/// wearing a purchase's name.
float unit_price_at(const world& w, entity_id body, std::size_t ri)
{
    const entity_id mid = lowest_market_on_body(w, body);
    if (mid == null_entity)
        return 0.0f;
    const market_component& mc = w.markets.at(mid);
    return mc.price[ri] > 0.0f ? mc.price[ri] : mc.base_price[ri];
}

/// One nation's network, tallied. INTEGER counts only, so the unordered-map
/// walks that fill it commute and the result is hash-layout-independent.
struct network_size
{
    int roads[3] = {0, 0, 0}; ///< Tiles at road_level 1 / 2 / 3.
    int hubs     = 0;         ///< Active ports + inland logistics hubs.
};

} // namespace

std::vector<network_purchase> derive_network_upkeep_claims(const world& w,
                                                           const std::map<entity_id, nation_budget>& budgets,
                                                           const network_upkeep_params& p,
                                                           std::vector<budget_claim>& claims)
{
    std::vector<network_purchase> out;

    // Unauthored rates mean no upkeep: no float is read and no claim appended,
    // so an unauthored world runs bit-identical to the pre-BL-643 build — the
    // `nation_budget` inertness discipline. (The scorer half needs no such
    // caveat here: `niche_logistics` has weighted this line since BL-542, so
    // the WEIGHT vector is unchanged by this TU — only the line's consumer is
    // new.)
    bool any_rate = std::isfinite(p.stone_per_hub) && p.stone_per_hub > 0.0f;
    any_rate = any_rate || (std::isfinite(p.timber_per_hub) && p.timber_per_hub > 0.0f);
    for (std::size_t i = 0; i < 3; ++i)
    {
        any_rate = any_rate || (std::isfinite(p.stone_per_level[i]) && p.stone_per_level[i] > 0.0f);
        any_rate = any_rate || (std::isfinite(p.timber_per_level[i]) && p.timber_per_level[i] > 0.0f);
    }
    // BL-742: an empty pool map no longer short-circuits — the market
    // fallback can buy from real inventory a world with no stocked pools
    // still holds.
    if (!any_rate || budgets.empty())
        return out;

    // ---- Tally the network, per nation --------------------------------------
    // GEOGRAPHY, derived from the world each tick — no persistent state, no
    // serialisation seam. Both source maps are unordered, but only integer
    // increments land here, which commute; the keyed std::map makes every walk
    // AFTER this point ascending-order.
    std::map<entity_id, network_size> sizes;
    for (const auto& [tile, nid] : w.tile_to_nation)
    {
        const auto tit = w.tiles.find(tile);
        if (tit == w.tiles.end())
            continue;
        const std::uint8_t lvl = tit->second.road_level;
        if (lvl >= 1 && lvl <= 3)
            ++sizes[nid].roads[lvl - 1];
    }
    for (const auto& [bid, bc] : w.buildings)
    {
        if (bc.type != building_type::port && bc.type != building_type::inland_logistics_hub)
            continue;
        if (bc.ticks_remaining > 0 || bc.decommissioned)
            continue; // only a standing, in-service hub is network
        const auto tnit = w.tile_to_nation.find(bc.tile);
        if (tnit == w.tile_to_nation.end())
            continue; // a hub on unclaimed ground is nobody's to maintain
        ++sizes[tnit->second].hubs;
    }
    if (sizes.empty())
        return out;

    // Stock already promised to an earlier claim THIS derivation, keyed
    // (corp, body, resource) — space_programme.cpp's reservation, so two
    // treasuries never buy the same units.
    std::map<std::tuple<entity_id, entity_id, std::size_t>, float> reserved;
    // BL-742: inventory already promised to an earlier MARKET fallback this
    // derivation, keyed (market, resource) — the same discipline one store over.
    std::map<std::pair<entity_id, std::size_t>, float> mkt_reserved;

    for (const auto& [nid, bud] : budgets) // std::map: ascending nation
    {
        const auto nit = w.nations.find(nid);
        if (nit == w.nations.end())
            continue;

        // BL-742: the line's share, in nation_budget.cpp's own arithmetic
        // (space_programme.cpp's derivation, same clamp, same filter, same
        // parenthesisation). Pool claims are funded by the pass itself and
        // need no share here; the MARKET fallback bypasses the claim
        // machinery (no corp payee exists), so it must cap ITSELF at the
        // share the pass would have granted, less what this nation's pool
        // claims already asked.
        float line_share = 0.0f;
        {
            const float reserve   = std::clamp(bud.reserve_fraction, 0.0f, 1.0f);
            const float spendable = nit->second.treasury * (1.0f - reserve);
            float weight_total = 0.0f;
            for (std::size_t i = 0; i < priority_count; ++i)
                if (std::isfinite(bud.weights[i]) && bud.weights[i] > 0.0f)
                    weight_total += bud.weights[i];
            const std::size_t li =
                static_cast<std::size_t>(budget_priority::logistics_maintenance);
            const float weight = (std::isfinite(bud.weights[li]) && bud.weights[li] > 0.0f)
                                 ? bud.weights[li] : 0.0f;
            if (spendable > 0.0f && weight_total > 0.0f)
                line_share = (spendable * weight) / weight_total;
        }
        float line_claimed = 0.0f;
        const auto sit = sizes.find(nid);
        if (sit == sizes.end())
            continue; // no network, no bill

        const network_size& ns = sit->second;

        // The bill per material, in a fixed accumulation order (three levels,
        // then hubs) over exact integer counts.
        const struct { resource_type good; float need; } goods[] = {
            { resource_type::stone,
              static_cast<float>(ns.roads[0]) * p.stone_per_level[0]
            + static_cast<float>(ns.roads[1]) * p.stone_per_level[1]
            + static_cast<float>(ns.roads[2]) * p.stone_per_level[2]
            + static_cast<float>(ns.hubs)     * p.stone_per_hub },
            { resource_type::timber,
              static_cast<float>(ns.roads[0]) * p.timber_per_level[0]
            + static_cast<float>(ns.roads[1]) * p.timber_per_level[1]
            + static_cast<float>(ns.roads[2]) * p.timber_per_level[2]
            + static_cast<float>(ns.hubs)     * p.timber_per_hub },
        };

        for (const auto& [good, need] : goods)
        {
            if (!std::isfinite(need) || !(need > 0.0f))
                continue;
            const std::size_t ri = static_cast<std::size_t>(good);

            // The supplier: the (corp, body) pool holding the MOST unreserved
            // stock — strict >, so ties keep the lowest key the std::map walk
            // reached first. The player's corp is never eligible (see the
            // header: a forced sale is an unsanctioned auto-action). Unlike
            // the space programme there is no whole-lump gate: upkeep is
            // continuous, so a pool short of the bill still supplies what it
            // holds and the bill is CAPPED to it.
            entity_id best_corp  = null_entity;
            entity_id best_body  = null_entity;
            float     best_avail = 0.0f;
            for (const auto& [key, pool] : w.corp_body_pools) // ascending (corp, body)
            {
                const entity_id corp = key.first;
                const entity_id body = key.second;
                if (corp == w.player_entity)
                    continue;
                if (w.corporations.find(corp) == w.corporations.end())
                    continue;
                if (w.bodies.find(body) == w.bodies.end())
                    continue;
                float avail = pool.quantities[ri];
                const auto rit = reserved.find(std::make_tuple(corp, body, ri));
                if (rit != reserved.end())
                    avail -= rit->second;
                if (avail > best_avail)
                {
                    best_corp  = corp;
                    best_body  = body;
                    best_avail = avail;
                }
            }
            if (best_corp == null_entity)
           
            {
                // BL-742: NO pool holds the good — the measured industrial-band
                // case, where auto-surplus sweeps every pool into market
                // inventory each tick and the pool-buying template starved
                // beside a stocked market. Fall back to the market with the
                // most unreserved REAL inventory (strict >, ties keep the
                // lowest id the sorted walk reached first), at that market's
                // own resolved price. No budget claim rides the machinery —
                // there is no corp payee — so the purchase caps itself at the
                // line share and settles as a direct treasury debit (the
                // standing unbacked-market simplification: the supplier was
                // already paid when auto-surplus sold the stock in).
                entity_id best_mkt   = null_entity;
                float     mkt_avail  = 0.0f;
                {
                    std::vector<entity_id> mids;
                    mids.reserve(w.markets.size());
                    for (const auto& [mid, mc] : w.markets)
                    {
                        (void)mc;
                        mids.push_back(mid);
                    }
                    std::sort(mids.begin(), mids.end());
                    for (const entity_id mid : mids)
                    {
                        const market_component& mc = w.markets.at(mid);
                        if (!(mc.base_price[ri] > 0.0f))
                            continue; // untradeable there: no price basis
                        float avail = mc.inventory[ri];
                        const auto mrit = mkt_reserved.find(std::make_pair(mid, ri));
                        if (mrit != mkt_reserved.end())
                            avail -= mrit->second;
                        if (avail > mkt_avail)
                        {
                            best_mkt  = mid;
                            mkt_avail = avail;
                        }
                    }
                }
                if (best_mkt == null_entity)
                    continue; // no pool AND no inventory anywhere: nothing to buy

                const market_component& mc = w.markets.at(best_mkt);
                const float unit = mc.price[ri] > 0.0f ? mc.price[ri] : mc.base_price[ri];
                if (!std::isfinite(unit) || !(unit > 0.0f))
                    continue;

                float quantity = std::min(need, mkt_avail);
                float amount   = quantity * unit;
                // The self-imposed share cap: never ask more than the line's
                // remaining share, scaled down rather than skipped (pro-rata
                // is this channel's whole shape).
                const float headroom = line_share - line_claimed;
                if (!(headroom > 0.0f))
                    continue;
                if (amount > headroom)
                {
                    quantity = headroom / unit;
                    amount   = headroom;
                }
                if (!std::isfinite(amount) || !(amount > 0.0f) || !(quantity > 0.0f))
                    continue;
                line_claimed += amount;
                mkt_reserved[std::make_pair(best_mkt, ri)] += quantity;

                network_purchase np;
                np.nation   = nid;
                np.supplier = null_entity; // THE MARKET
                np.market   = best_mkt;
                np.body     = mc.body;
                np.resource = good;
                np.quantity = quantity;
                np.credits  = amount;
                out.push_back(np);
                continue;
            }

            const float unit = unit_price_at(w, best_body, ri);
            if (!std::isfinite(unit) || !(unit > 0.0f))
                continue; // no price basis, no purchase

            const float quantity = std::min(need, best_avail);
            const float amount   = quantity * unit;
            if (!std::isfinite(amount) || !(amount > 0.0f))
                continue;

            reserved[std::make_tuple(best_corp, best_body, ri)] += quantity;
            line_claimed += amount;

            budget_claim c;
            c.nation = nid;
            c.corp   = best_corp;
            c.line   = budget_priority::logistics_maintenance;
            c.amount = amount;
            // No subject: this line takes none (`line_takes_subject`), and the
            // gather nulls the field on such a line anyway — the pro-rata fill
            // is the point (see the header).
            claims.push_back(c);

            network_purchase np;
            np.nation   = nid;
            np.supplier = best_corp;
            np.body     = best_body;
            np.resource = good;
            np.quantity = quantity;
            np.credits  = amount;
            out.push_back(np);
        }
    }
    return out;
}

void settle_network_purchases(world& w,
                              std::vector<network_purchase>& purchases,
                              const national_budget_tick& tick)
{
    // ---- BL-742: the MARKET-fallback intents settle first, independent of
    // the transfer record — no claim rode the machinery, so the treasury is
    // debited directly here, bounded by what it still holds (the garrison
    // precedent for a direct debit), pro-rata when short. The goods leave the
    // market's REAL inventory; the money leaves the world, exactly as every
    // corp's own market purchase does (money_conservation's documented
    // unbacked-market simplification). Intent order is derive order — sorted
    // nations, fixed goods order — so the walk is deterministic.
    for (network_purchase& np : purchases)
    {
        if (np.supplier != null_entity)
            continue; // a pool purchase: the transfer loop below owns it
        const auto nit = w.nations.find(np.nation);
        const auto mit = w.markets.find(np.market);
        if (nit == w.nations.end() || mit == w.markets.end())
            continue;
        if (!(np.credits > 0.0f) || !(np.quantity > 0.0f))
            continue;

        const float treasury_room = std::max(0.0f, nit->second.treasury);
        const float pay  = std::min(np.credits, treasury_room);
        if (!(pay > 0.0f))
            continue;
        float fill  = pay / np.credits;
        float drawn = np.quantity * fill;

        // Defend the window between derive and settle exactly as the pool
        // path defends its reservation: never draw more than the shelf holds.
        const std::size_t ri = static_cast<std::size_t>(np.resource);
        if (mit->second.inventory[ri] < drawn)
        {
            drawn = std::max(0.0f, mit->second.inventory[ri]);
            fill  = (np.quantity > 0.0f) ? drawn / np.quantity : 0.0f;
        }
        if (!(drawn > 0.0f))
            continue;

        mit->second.inventory[ri] -= drawn;
        nit->second.treasury      -= np.credits * fill;
        np.funded    = true;
        np.paid      = np.credits * fill;
        np.drawn     = drawn;
        np.completed = true;
    }

    if (tick.transfers.empty())
        return;

    for (const budget_transfer& t : tick.transfers) // the record's stored order
    {
        if (t.line != budget_priority::logistics_maintenance || !(t.credits > 0.0f))
            continue;

        // The first unfunded intent this transfer is FOR, by (nation, supplier)
        // in sequence. Credits are NOT compared — the pass may have paid any
        // pro-rata fraction of the claim — but the sequence match is still
        // exact: the gather pins arrival order within a corp's claims and the
        // record keeps arrival order within equal (corp, nation, line) keys,
        // so one supplier's stone and timber transfers arrive in the order the
        // intents were emitted.
        network_purchase* match = nullptr;
        for (network_purchase& np : purchases)
        {
            if (np.funded)
                continue;
            if (np.nation != t.nation || np.supplier != t.corp)
                continue;
            match = &np;
            break;
        }

        if (match == nullptr)
        {
            // A PAID logistics transfer with no intent behind it. In-process
            // this derivation is the line's only claimant (corp_ai emits only
            // public_exploration claims), so this is unreachable — but the
            // claim vector is an AI-facing seam, wire-reachable over --serve,
            // and a rogue claim here would otherwise leave credits on a corp
            // with no goods drawn and no ledger row. Claw it back in the same
            // two places the pass wrote it — space_programme.cpp's defence.
            const auto cit = w.corporations.find(t.corp);
            const auto nit = w.nations.find(t.nation);
            if (cit != w.corporations.end()) cit->second.balance -= t.credits;
            if (nit != w.nations.end())      nit->second.treasury += t.credits;
            continue;
        }

        network_purchase& np = *match;
        np.funded = true;

        // The transfer's own per-claim fill decides the draw. Guarded as the
        // value that lands: a non-finite or non-positive fill (impossible from
        // the pass, which only records pay > 0) claws back rather than drawing
        // garbage; capped at 1 so a draw never exceeds the reserved quantity.
        const float fill = (std::isfinite(t.fill_fraction) && t.fill_fraction > 0.0f)
                           ? std::min(t.fill_fraction, 1.0f) : 0.0f;
        const float drawn = np.quantity * fill;

        const std::size_t ri  = static_cast<std::size_t>(np.resource);
        const auto        pit = w.corp_body_pools.find(std::make_pair(np.supplier, np.body));
        if (fill > 0.0f && pit != w.corp_body_pools.end()
            && pit->second.quantities[ri] >= drawn)
        {
            // The terminal sink: the materials leave the supplier's pool and
            // land nowhere — the repairs went into the roadbed. The credit
            // half stays on the supplier's balance (it is a sale) and
            // run_nation_step folds it onto `subsidies` so `net()` explains
            // the delta.
            pit->second.quantities[ri] -= drawn;
            np.paid      = t.credits;
            np.drawn     = drawn;
            np.completed = true;
        }
        else
        {
            // The derivation's reservation makes this unreachable within one
            // tick; defend it anyway, exactly as space_programme.cpp does:
            // reverse the transfer in the same two places the pass wrote it,
            // so the tick's books still balance and the nation did not pay
            // for a repair that never happened.
            const auto cit = w.corporations.find(t.corp);
            const auto nit = w.nations.find(t.nation);
            if (cit != w.corporations.end()) cit->second.balance -= t.credits;
            if (nit != w.nations.end())      nit->second.treasury += t.credits;
        }
    }
}
