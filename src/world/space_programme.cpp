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

/// The procurement price basis: the resolved price at the supplier pool's
/// market (BL-1003: @p pool_key is that market; a body key — a market-less
/// body — finds none), or `base_price` before first resolution —
/// `request_quote`'s own "what the good actually costs here" reading, with no
/// volume discount (the state pays spot). Zero when there is no market at all:
/// a purchase with no price basis is refused, never priced at nothing — a
/// zero-credit draw would be confiscation wearing a purchase's name.
float unit_price_at(const world& w, entity_id pool_key, std::size_t ri)
{
    const entity_id mid = (w.markets.find(pool_key) != w.markets.end())
        ? pool_key : lowest_market_on_body(w, pool_key);
    if (mid == null_entity)
        return 0.0f;
    const market_component& mc = w.markets.at(mid);
    return posted_price(mc, ri); // BL-1172: the posted price, the one rule
}

/// BL-1172 (Ben, 2026-10-03: the ceiling governs EVERY draw — "yes, every
/// draw"): may the state buy good @p ri out of a corp pool keyed @p pool_key?
/// The pool is priced at its market (`unit_price_at`'s market), so the same
/// fair-price ceiling the shelf fallback obeys applies to it: posted price at
/// or under `reservation_mult x base`. A pool with no market to price it is
/// left to `unit_price_at`, which refuses it at zero. 0 = no ceiling.
bool pool_price_admitted(const world& w, entity_id pool_key, std::size_t ri, float reservation_mult)
{
    const entity_id mid = (w.markets.find(pool_key) != w.markets.end())
        ? pool_key : lowest_market_on_body(w, pool_key);
    if (mid == null_entity)
        return true;
    return shelf_admits(w.markets.at(mid), ri, reservation_mult, /*off_buys=*/true);
}

} // namespace

std::vector<space_purchase> derive_space_programme_claims(const world& w,
                                                          const std::map<entity_id, nation_budget>& budgets,
                                                          const space_programme_params& p,
                                                          float reservation_mult,
                                                          std::vector<budget_claim>& claims,
                                                          std::vector<market_want>* wants)
{
    // BL-1227: one want row per good the line wanted, filled or not.
    const auto want = [&](entity_id nation, entity_id market, resource_type good, float q) {
        if (wants != nullptr && q > 0.0f)
            wants->push_back({nation, market, good, q});
    };
    std::vector<space_purchase> out;

    // Unauthored lumps mean no programme: the pass reads no float and appends
    // no claim — THIS DERIVATION is inert without the Lua table. The build as
    // a whole is not (cold-review finding, 2026-09-01): `calm_space` is a C++
    // default in nation_ai_params, so every scored nation emits a ten-line
    // normalised weight vector regardless, all nine existing lines' shares
    // shrink, and state_hash moves on any world with scored nations. The same
    // per-pass inertness `run_national_budget` states for an empty budget map.
    const bool any_lump = (std::isfinite(p.components_lump) && p.components_lump > 0.0f)
                       || (std::isfinite(p.propellant_lump) && p.propellant_lump > 0.0f);
    // BL-742: an empty pool map no longer short-circuits — the market
    // fallback can buy a whole lump off a real shelf.
    if (!any_lump || budgets.empty())
        return out;

    // The two goods, in the fixed authored order the claims are emitted in.
    const std::pair<resource_type, float> goods[] = {
        { resource_type::spacecraft_components, p.components_lump },
        { resource_type::propellant,            p.propellant_lump },
    };

    // Stock already promised to an earlier claim THIS derivation, keyed
    // (corp, pool key, resource). Two nations walked in ascending id never claim
    // the same units, so a funded claim always finds its lump at settlement.
    std::map<std::tuple<entity_id, entity_id, std::size_t>, float> reserved;
    // BL-742: inventory promised to an earlier MARKET fallback, (market, resource).
    std::map<std::pair<entity_id, std::size_t>, float> mkt_reserved;

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

            // BL-1265 (MARKETS.md § The shelf economy): the state BUYS ITS LUMP
            // OFF A SHELF. Corporations hold no stockpiles, so there is no
            // corporation pool to buy from; the supplier is always a market.
            {
                // The lump discipline (BL-742): one market must hold the WHOLE lump (the state
                // splits no launch across shelves), the purchase bypasses the
                // claim machinery (no corp payee), caps itself at the line's
                // remaining share, and settles as a direct whole-or-nothing
                // treasury debit. The supplier was already paid when the stock
                // sold in; the money leaves the world as every market purchase
                // does (money_conservation's documented simplification).
                entity_id best_mkt  = null_entity;
                float     mkt_avail = 0.0f;
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
                            continue;
                        // BL-1172: the same fair-price ceiling every goods
                        // draw obeys (Ben, 2026-10-03).
                        if (!shelf_admits(mc, ri, reservation_mult, /*off_buys=*/true))
                            continue;
                        float avail = mc.inventory[ri];
                        const auto mrit = mkt_reserved.find(std::make_pair(mid, ri));
                        if (mrit != mkt_reserved.end())
                            avail -= mrit->second;
                        if (avail >= lump && avail > mkt_avail)
                        {
                            best_mkt  = mid;
                            mkt_avail = avail;
                        }
                    }
                }
                if (best_mkt == null_entity)
                {
                    want(nid, null_entity, good, lump); // BL-1227: wanted, nowhere held
                    continue; // no pool and no shelf holds a whole lump
                }
                want(nid, best_mkt, good, lump); // BL-1227: wanted at that shelf

                const market_component& mc = w.markets.at(best_mkt);
                const float unit = posted_price(mc, ri); // BL-1172: the posted price
                if (!std::isfinite(unit) || !(unit > 0.0f))
                    continue;
                const float amount = lump * unit;
                if (!std::isfinite(amount) || !(amount > 0.0f))
                    continue;
                if (amount > share - line_claimed)
                    continue; // the lump gate, unchanged: whole or not at all
                line_claimed += amount;
                mkt_reserved[std::make_pair(best_mkt, ri)] += lump;

                space_purchase sp;
                sp.nation   = nid;
                sp.supplier = null_entity; // THE MARKET
                sp.market   = best_mkt;
                sp.body     = mc.body;
                sp.resource = good;
                sp.quantity = lump;
                sp.credits  = amount;
                out.push_back(sp);
            }
        }
    }
    return out;
}

void settle_space_purchases(world& w,
                            std::vector<space_purchase>& purchases,
                            const national_budget_tick& tick)
{
    // ---- BL-742: MARKET-fallback lumps settle first, independent of the
    // transfer record — no claim rode the machinery, so the treasury is
    // debited directly, WHOLE OR NOTHING (the lump property survives the
    // fallback). The goods leave the market's real inventory; the money
    // leaves the world as every market purchase does. Intent order is derive
    // order — deterministic.
    for (space_purchase& sp : purchases)
    {
        if (sp.supplier != null_entity)
            continue; // a pool lump: the transfer loop below owns it
        const auto nit = w.nations.find(sp.nation);
        const auto mit = w.markets.find(sp.market);
        if (nit == w.nations.end() || mit == w.markets.end())
            continue;
        const std::size_t ri = static_cast<std::size_t>(sp.resource);
        if (!(sp.credits > 0.0f) || !(sp.quantity > 0.0f))
            continue;
        if (nit->second.treasury < sp.credits)
            continue; // whole or nothing: the share banked, the lump waits
        if (mit->second.inventory[ri] < sp.quantity)
            continue; // the shelf thinned between derive and settle: no partial launch

        mit->second.inventory[ri] -= sp.quantity;
        nit->second.treasury      -= sp.credits;
        sp.funded    = true;
        sp.completed = true;
    }

    if (tick.transfers.empty())
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
        space_purchase* match = nullptr;
        for (space_purchase& sp : purchases)
        {
            if (sp.funded)
                continue;
            if (sp.nation != t.nation || sp.supplier != t.corp || sp.body != t.subject)
                continue;
            if (sp.credits != t.credits)
                continue;
            match = &sp;
            break;
        }

        if (match == nullptr)
        {
            // A PAID space transfer with no intent behind it. In-process the
            // derivation is the line's only claimant, so this is unreachable —
            // but the claim vector is an AI-facing seam (`nation_budget.hpp`:
            // claims become wire-reachable over --serve), and a rogue claim on
            // this line would otherwise leave credits on a corp with no goods
            // drawn, no `subsidies` row and no `space_purchases` row: a silent
            // transfer `net()` cannot explain (cold-review finding 4,
            // 2026-09-01). Claw it back in the same two places the pass wrote
            // it, exactly as the failed survey earmark is defended.
            const auto cit = w.corporations.find(t.corp);
            const auto nit = w.nations.find(t.nation);
            if (cit != w.corporations.end()) cit->second.balance -= t.credits;
            if (nit != w.nations.end())      nit->second.treasury += t.credits;
            continue;
        }

        space_purchase& sp = *match;
        sp.funded = true;

        // BL-1265: a corporation-supplied lump no longer exists (no pools), so
        // a transfer matched to one cannot be settled in goods. Reverse it in
        // the same two places the pass wrote it, as the failed survey earmark
        // is defended (nation_step.cpp), so the books balance and the nation
        // did not pay for a launch that never happened.
        {
            const auto cit = w.corporations.find(t.corp);
            const auto nit = w.nations.find(t.nation);
            if (cit != w.corporations.end()) cit->second.balance -= t.credits;
            if (nit != w.nations.end())      nit->second.treasury += t.credits;
        }
    }
}
