#include "space_programme.hpp"

#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <tuple>
#include <utility>

namespace {

/// The lowest-id market on @p body, or null. The same deterministic pick
/// `any_market_on_body` (corp_command.cpp) makes; a file-scope copy because
/// that helper is private to the command seam and this TU must stay linkable
/// without it.
entity_id lowest_market_on_body(const world& w, entity_id body)
{
    entity_id best = null_entity;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body && (best == null_entity || mid < best))
            best = mid;
    return best;
}

/// The procurement price basis: the resolved price at @p body's market, or
/// `base_price` before first resolution — `request_quote`'s own "what the good
/// actually costs here" reading, with no volume discount (the state pays spot).
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

} // namespace

std::vector<space_purchase> derive_space_programme_claims(const world& w,
                                                          const std::map<entity_id, nation_budget>& budgets,
                                                          const space_programme_params& p,
                                                          std::vector<budget_claim>& claims)
{
    std::vector<space_purchase> out;

    // Unauthored lumps mean no programme: the pass reads no float and appends
    // no claim, so an economy.lua without the table runs bit-identical to the
    // pre-BL-644 build. The same inertness `run_national_budget` states for an
    // empty budget map.
    const bool any_lump = (std::isfinite(p.components_lump) && p.components_lump > 0.0f)
                       || (std::isfinite(p.propellant_lump) && p.propellant_lump > 0.0f);
    if (!any_lump || budgets.empty() || w.corp_body_pools.empty())
        return out;

    // The two goods, in the fixed authored order the claims are emitted in.
    const std::pair<resource_type, float> goods[] = {
        { resource_type::spacecraft_components, p.components_lump },
        { resource_type::propellant,            p.propellant_lump },
    };

    // Stock already promised to an earlier claim THIS derivation, keyed
    // (corp, body, resource). Two nations walked in ascending id never claim
    // the same units, so a funded claim always finds its lump at settlement.
    std::map<std::tuple<entity_id, entity_id, std::size_t>, float> reserved;

    for (const auto& [nid, bud] : budgets) // std::map: ascending nation
    {
        const auto nit = w.nations.find(nid);
        if (nit == w.nations.end() || !(nit->second.treasury > 0.0f))
            continue;

        // The line's share, in nation_budget.cpp's own arithmetic — the same
        // clamp, the same finite-positive weight filter, the same enum-order
        // sum, and the same `(spendable * weight) / weight_total`
        // parenthesisation — so a claim gated as affordable here is bit-for-bit
        // the claim rule 3a then pays. (Its two sibling recomputations,
        // `contracted_force_share` and the garrison bill, group the divide
        // differently; they can, because no claim of theirs ever re-enters the
        // pass to be compared against the pass's own share.)
        const float reserve   = std::clamp(bud.reserve_fraction, 0.0f, 1.0f);
        const float spendable = nit->second.treasury * (1.0f - reserve);
        if (!(spendable > 0.0f))
            continue;

        float weight_total = 0.0f;
        for (std::size_t i = 0; i < priority_count; ++i)
            if (std::isfinite(bud.weights[i]) && bud.weights[i] > 0.0f)
                weight_total += bud.weights[i];
        if (!(weight_total > 0.0f))
            continue;

        const std::size_t li = static_cast<std::size_t>(budget_priority::space_programme);
        const float weight = (std::isfinite(bud.weights[li]) && bud.weights[li] > 0.0f)
                             ? bud.weights[li] : 0.0f;
        const float share = (spendable * weight) / weight_total;
        if (!(share > 0.0f))
            continue;

        // What this nation's claims have already asked of the line this tick,
        // so the second good is gated on the share the first one left — the
        // derivation never asks for a lump it has already spent the share of.
        float line_claimed = 0.0f;

        for (const auto& [good, lump] : goods)
        {
            if (!std::isfinite(lump) || !(lump > 0.0f))
                continue;
            const std::size_t ri = static_cast<std::size_t>(good);

            // The supplier: the (corp, body) pool holding the MOST unreserved
            // stock that covers a WHOLE lump — strict >, so ties keep the
            // lowest key the std::map walk reached first. The player's corp is
            // never eligible (see the header: a forced sale is an unsanctioned
            // auto-action on the player's corp).
            entity_id best_corp = null_entity;
            entity_id best_body = null_entity;
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
                    continue; // the claim's subject must survive the gather check
                float avail = pool.quantities[ri];
                const auto rit = reserved.find(std::make_tuple(corp, body, ri));
                if (rit != reserved.end())
                    avail -= rit->second;
                if (avail >= lump && avail > best_avail)
                {
                    best_corp  = corp;
                    best_body  = body;
                    best_avail = avail;
                }
            }
            if (best_corp == null_entity)
                continue; // nobody holds a whole lump: the state splits no launch

            const float unit = unit_price_at(w, best_body, ri);
            if (!std::isfinite(unit) || !(unit > 0.0f))
                continue; // no price basis, no purchase

            const float amount = lump * unit;
            if (!std::isfinite(amount) || !(amount > 0.0f))
                continue;

            // The lump gate: claim only what the line's remaining share
            // covers. Rule 3a would skip an oversized claim anyway; gating
            // here as well keeps the RESERVATION honest — a nation that
            // cannot pay does not hold stock against one that can. The share
            // keeps accumulating (rule 2), so the lump fires on a later tick.
            if (amount > share - line_claimed)
                continue;
            line_claimed += amount;

            reserved[std::make_tuple(best_corp, best_body, ri)] += lump;

            budget_claim c;
            c.nation  = nid;
            c.corp    = best_corp;
            c.line    = budget_priority::space_programme;
            c.amount  = amount;
            c.subject = best_body;
            claims.push_back(c);

            space_purchase sp;
            sp.nation   = nid;
            sp.supplier = best_corp;
            sp.body     = best_body;
            sp.resource = good;
            sp.quantity = lump;
            sp.credits  = amount;
            out.push_back(sp);
        }
    }
    return out;
}

void settle_space_purchases(world& w,
                            std::vector<space_purchase>& purchases,
                            const national_budget_tick& tick)
{
    if (purchases.empty() || tick.transfers.empty())
        return;

    for (const budget_transfer& t : tick.transfers) // the record's stored order
    {
        if (t.line != budget_priority::space_programme || !(t.credits > 0.0f))
            continue;

        // The first unfunded intent this transfer is FOR. Credits compare with
        // == deliberately: an earmarked claim is paid whole (rule 3a), so the
        // transfer carries the intent's own float back unchanged. Two intents
        // on one (nation, supplier, body) with equal credits — both goods at
        // coincidentally equal lump prices — resolve in emission order, which
        // is also the transfers' stable arrival order within an equal sort key.
        for (space_purchase& sp : purchases)
        {
            if (sp.funded)
                continue;
            if (sp.nation != t.nation || sp.supplier != t.corp || sp.body != t.subject)
                continue;
            if (sp.credits != t.credits)
                continue;

            sp.funded = true;

            const std::size_t ri  = static_cast<std::size_t>(sp.resource);
            const auto        pit = w.corp_body_pools.find(std::make_pair(sp.supplier, sp.body));
            if (pit != w.corp_body_pools.end() && pit->second.quantities[ri] >= sp.quantity)
            {
                // The terminal sink: the lump leaves the supplier's pool and
                // is credited to nobody — the satellite launched. The credit
                // half already landed on the supplier's balance and STAYS
                // there; it is a sale, and run_nation_step folds it onto the
                // corp's `subsidies` line so `net()` explains the delta.
                pit->second.quantities[ri] -= sp.quantity;
                sp.completed = true;
            }
            else
            {
                // The derivation's reservation makes this unreachable within
                // one tick; defend it anyway, exactly as the failed survey
                // earmark is defended (nation_step.cpp): reverse the transfer
                // in the same two places the pass wrote it, so the tick's
                // books still balance and the nation did not pay for a launch
                // that never happened.
                const auto cit = w.corporations.find(t.corp);
                const auto nit = w.nations.find(t.nation);
                if (cit != w.corporations.end()) cit->second.balance -= t.credits;
                if (nit != w.nations.end())      nit->second.treasury += t.credits;
            }
            break;
        }
    }
}
