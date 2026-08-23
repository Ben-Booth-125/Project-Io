#include "nation_budget.hpp"

#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <utility>
#include <vector>

namespace {

/// One nation's gathered claims, bucketed by line and pinned into the pass's
/// walk order. `first` is the payee corp, `second` the credits asked for.
using line_claims = std::array<std::vector<std::pair<entity_id, float>>, priority_count>;

} // namespace

void run_national_budget(world& w,
                         const std::map<entity_id, nation_budget>& budgets,
                         const std::vector<budget_claim>& claims,
                         national_budget_tick* report)
{
    if (report)
        *report = national_budget_tick{};

    // Nothing authored means nothing spent. The early-out is not an
    // optimisation: it is the INERTNESS guarantee stated where it is provable —
    // no float in the world is read, let alone written, so a world in which no
    // nation authors a budget runs bit-identical to the pre-BL-537 build.
    if (budgets.empty() || claims.empty())
        return;

    // ---- Gather -----------------------------------------------------------
    // Claims arrive in the producer's order, which this pass deliberately does
    // not trust: a caller that walked `w.corporations` (an unordered_map) would
    // hand over a hash-layout-dependent sequence, and a float sum over it is
    // then platform-dependent. Bucket by (nation, line) — `std::map`, so the
    // nations come out ascending — then stable_sort each bucket by corp id, so
    // the accumulation order is (nation, line, corp, arrival) always.
    //
    // A claim is DROPPED HERE, before any arithmetic, when it names a corp that
    // does not exist, a nation with no authored budget, a line outside the enum,
    // or a non-positive / non-finite amount. Dropping at gather time is the
    // point: a claim admitted into the demand total and then found unpayable
    // would debit a treasury with nothing to credit, which is exactly the money
    // destruction BL-480 removed from the levy.
    //
    // THE LINE CHECK IS NOT DEFENSIVE PADDING. `budget_priority` has a `uint8_t`
    // underlying type, so every one of 0..255 is a valid VALUE of the enum while
    // only 0..8 is a valid INDEX into a `priority_count`-long array. Without the
    // check the gather loop writes out of bounds on any claim carrying a line it
    // did not author — AddressSanitizer confirmed the write (Sprint N1 adversarial
    // pass, 2026-08-22). Claims are producer-supplied and BL-538's consumers are
    // in-process today, but the standing rule is that an AI-facing seam is an
    // untrusted input boundary: the moment a claim arrives over `--serve` this is
    // a wire-reachable overflow. Validate as the value that lands in the
    // destination, and drop the whole claim rather than clamping it onto a line
    // nobody asked for.
    std::map<entity_id, line_claims> by_nation;
    for (const budget_claim& c : claims)
    {
        if (c.corp == null_entity || c.nation == null_entity)
            continue;
        if (!std::isfinite(c.amount) || c.amount <= 0.0f)
            continue;
        if (static_cast<std::size_t>(c.line) >= priority_count)
            continue;
        if (budgets.find(c.nation) == budgets.end())
            continue;
        if (w.nations.find(c.nation) == w.nations.end())
            continue;
        if (w.corporations.find(c.corp) == w.corporations.end())
            continue;

        by_nation[c.nation][static_cast<std::size_t>(c.line)].emplace_back(c.corp, c.amount);
    }
    for (auto& [nation, lines] : by_nation)
        for (auto& bucket : lines)
            std::stable_sort(bucket.begin(), bucket.end(),
                             [](const auto& a, const auto& b) { return a.first < b.first; });

    // The trap budget_system.cpp's levy pass already documents (BL-480 review
    // fix, 2026-08-19 #2): the corp credits are a CROSS-corp accumulation into
    // an unordered_map, and float addition is not associative, so applying them
    // mid-walk would make each balance depend on iteration order. Collect
    // (corp, nation, amount) here and apply after the walk, sorted. The treasury
    // debit stays per-nation and inside the walk, where its order is already
    // pinned.
    std::vector<std::tuple<entity_id, entity_id, float>> corp_credits;

    // ---- Allocate ---------------------------------------------------------
    for (const auto& [nation, budget] : budgets)
    {
        const auto nat = w.nations.find(nation);
        if (nat == w.nations.end())
            continue;

        nation_budget_result res;
        res.nation   = nation;
        res.treasury = nat->second.treasury;

        // Rule 3, first half: a nation cannot allocate what it does not hold.
        // A treasury at (or below) zero spends nothing and errors nowhere.
        if (!(res.treasury > 0.0f))
        {
            if (report)
                report->nations.push_back(res);
            continue;
        }

        // Rule 2: the reserve is withheld before anything is allotted. Clamped
        // rather than rejected, because an out-of-range authored dial should
        // read as "save everything" / "save nothing" and not as a hole.
        const float reserve = std::clamp(budget.reserve_fraction, 0.0f, 1.0f);
        res.spendable = res.treasury * (1.0f - reserve);

        // Weights are read as fractions of the spendable total, so the sum is
        // needed before any share is. Summed in the fixed enum order — the same
        // order the shares are then computed in. An all-zero set (the default)
        // spends nothing: that IS the inertness condition, not a special case.
        float weight_total = 0.0f;
        for (std::size_t i = 0; i < priority_count; ++i)
            if (std::isfinite(budget.weights[i]) && budget.weights[i] > 0.0f)
                weight_total += budget.weights[i];

        if (!(res.spendable > 0.0f) || !(weight_total > 0.0f))
        {
            if (report)
                report->nations.push_back(res);
            continue;
        }

        const auto claims_it = by_nation.find(nation);

        // ---- The bound ----------------------------------------------------
        // Every share is derived from `spendable` independently, so their sum
        // can exceed it by a float ULP even with a normalised weight set. What
        // holds the total down is `nation_paid` — the running total of what this
        // nation has actually paid, accumulated UPWARD from zero — with the
        // headroom re-derived from it on every pay.
        //
        // It is deliberately NOT a decremented remainder, which is the shape this
        // pass shipped with and is unsound: `remaining -= pay` rounds straight
        // back to `remaining` once `pay` falls below half a ULP of it, so a
        // nation facing enough small claims pays every one of them while its
        // bound never moves. An overdraw, with a bound in plain sight. (Sprint N1
        // adversarial pass, 2026-08-22 — defect 2, against rule 3's "never
        // overdrawn".)
        //
        // It is also deliberately not a per-transfer debit off the live treasury,
        // which fails the same way in the other direction and worse: a 1e-5 pay
        // against a treasury of 1000 is BELOW half a ULP of it, so the debit
        // rounds away to nothing while the credit lands in full — money created,
        // which is the leak this item exists to not have. Small quantities are
        // summed together FIRST and applied to the large one ONCE.
        //
        // The residual: `nation_paid` itself stalls if a single pay falls below
        // half a ULP of the running total, which needs the claim count on one
        // nation in one tick to reach ~2^24. That is not a world Io generates,
        // and unlike the two shapes above it is a bound stated rather than a
        // bound assumed.
        float nation_paid = 0.0f;

        for (std::size_t i = 0; i < priority_count; ++i)
        {
            budget_line_result& line = res.lines[i];

            const float weight = (std::isfinite(budget.weights[i]) && budget.weights[i] > 0.0f)
                                 ? budget.weights[i] : 0.0f;
            // Normalise on read, once, in one place: an authored set that
            // already sums to 1.0 divides by exactly 1.0f and is untouched.
            line.share = (res.spendable * weight) / weight_total;

            if (claims_it == by_nation.end())
                continue; // no consumer on any line — nothing transfers, nothing is lost

            const auto& bucket = claims_it->second[i];
            if (bucket.empty())
                continue; // BL-538 owns what a line buys; a line with no consumer
                          // yet transfers to nobody and is simply skipped

            for (const auto& [corp, amount] : bucket)
                line.demand += amount;

            if (!(line.demand > 0.0f) || !(line.share > 0.0f))
                continue;

            // Rule 3, second half: where demand exceeds the share the line is
            // PARTIALLY FILLED pro rata and says so — every claimant takes the
            // same fraction. Rationing in claim order instead would hand the
            // lowest corp id a funding advantage nobody authored.
            const bool  ration = line.demand > line.share;
            const float scale  = ration ? (line.share / line.demand) : 1.0f;
            line.rationed      = ration;

            for (const auto& [corp, amount] : bucket)
            {
                const float headroom = res.spendable - nation_paid;
                if (!(headroom > 0.0f))
                    break; // this nation has spent its whole allotment

                float pay = ration ? (amount * scale) : amount;
                pay = std::min(pay, headroom);
                if (!(pay > 0.0f))
                    continue;

                nation_paid += pay;
                line.paid   += pay;

                // Rule 1: a direct transfer to a NAMED corporation. This credit
                // and the treasury's debit below are the same float — no market,
                // no clearing, no price.
                corp_credits.emplace_back(corp, nation, pay);
            }

            line.fill_fraction = line.paid / line.demand;
        }

        // The debit half, applied ONCE per nation, so the treasury's delta and the
        // reported total are the SAME float rather than two accumulations that
        // agree only approximately — and so that a tick's many small transfers
        // are summed among themselves before they meet a treasury large enough to
        // absorb them individually.
        //
        // `nation_paid <= spendable <= treasury` by the headroom bound above, so
        // this subtraction cannot drive the treasury negative.
        if (nation_paid != 0.0f)
        {
            nat->second.treasury -= nation_paid;
            res.paid = nation_paid;
            if (report)
                report->total_transferred += nation_paid;
        }

        // Rule 2, restated as arithmetic: nothing else leaves. Whatever the
        // lines did not spend is still sitting in `treasury`, so next tick's
        // spendable — and therefore every line's share — is larger.

        if (report)
            report->nations.push_back(res);
    }

    // ---- Transfer ---------------------------------------------------------
    // (corp, nation, amount) order, so the total each corp receives is
    // independent of unordered_map iteration. Every payee was verified to exist
    // at gather time; re-find defensively anyway, mirroring the levy pass.
    if (!corp_credits.empty())
    {
        std::sort(corp_credits.begin(), corp_credits.end());
        for (const auto& [corp, nation, amount] : corp_credits)
        {
            const auto cit = w.corporations.find(corp);
            if (cit != w.corporations.end())
                cit->second.balance += amount;
        }
    }
}
