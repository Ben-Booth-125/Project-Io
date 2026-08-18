#include "condition_set.hpp"

#include "unit_roster.hpp" // BL-459: unit_strength (strength is derived, not stored)
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

/// Every market entity id, ascending. Markets live in an `unordered_map`, and
/// the `market` subject sums float prices across them — so the summation order
/// has to be pinned or two structurally-identical worlds could differ by a float
/// ULP. Every other subject sums integers or iterates an ordered container.
std::vector<entity_id> market_ids_sorted(const world& w)
{
    std::vector<entity_id> ids;
    ids.reserve(w.markets.size());
    for (const auto& [id, mc] : w.markets)
        ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

} // namespace

bool condition_subject_is_integral(condition_subject s)
{
    switch (s)
    {
        case condition_subject::research:
        case condition_subject::structure:
        case condition_subject::era:
        case condition_subject::military_units:
            return true;
        case condition_subject::stockpile:
        case condition_subject::market:
        case condition_subject::surplus:
        case condition_subject::military_strength:
            return false;
    }
    return false;
}

float measure_condition(const condition& c, const world& w, entity_id subject_corp)
{
    const auto corp_it = w.corporations.find(subject_corp);
    if (corp_it == w.corporations.end())
        return 0.0f; // An unknown corp measures zero on every subject, never throws.
    const corporation_component& cc = corp_it->second;

    switch (c.subject)
    {
        case condition_subject::research:
            return w.has_tech(subject_corp, c.key) ? 1.0f : 0.0f;

        case condition_subject::structure:
        {
            int n = 0;
            for (const entity_id bid : cc.assets) // vector: authored order, deterministic
            {
                const auto bit = w.buildings.find(bid);
                if (bit != w.buildings.end() && bit->second.type == c.structure)
                    ++n;
            }
            return static_cast<float>(n);
        }

        case condition_subject::stockpile:
        {
            // corp_body_pools is a std::map, so this walks in key order.
            const std::size_t ri = static_cast<std::size_t>(c.resource);
            float total = 0.0f;
            for (const auto& [key, pool] : w.corp_body_pools)
                if (key.first == subject_corp)
                    total += pool.quantities[ri];
            return total;
        }

        case condition_subject::market:
        {
            // Mean resolved price across every market in the world — a law or a
            // tech asking "is this good expensive yet?" is asking a world-level
            // question, not a per-market one. Summed in ascending id order.
            const std::size_t ri  = static_cast<std::size_t>(c.resource);
            const auto        ids = market_ids_sorted(w);
            if (ids.empty())
                return 0.0f;
            float total = 0.0f;
            for (const entity_id id : ids)
                total += w.markets.at(id).price[ri];
            return total / static_cast<float>(ids.size());
        }

        case condition_subject::surplus:
            return cc.balance;

        case condition_subject::era:
        {
            // HONEST CAVEAT: ERAS.md's Era system is designed and NOT implemented
            // (that doc opens with a status banner). In code, space access is
            // gated on launchpad presence and nothing else — so that is what this
            // measures: a corp that owns a launchpad reads Era 1, otherwise Era 0.
            // When the Era system lands this becomes a lookup, and every authored
            // `era` condition keeps its meaning.
            for (const entity_id bid : cc.assets)
            {
                const auto bit = w.buildings.find(bid);
                if (bit != w.buildings.end() && bit->second.type == building_type::launchpad)
                    return 1.0f;
            }
            return 0.0f;
        }

        case condition_subject::military_units:
        {
            long long n = 0; // integer sum: order-independent by construction
            for (const auto& [uid, uc] : w.units)
                if (uc.owner == subject_corp)
                    n += uc.count;
            return static_cast<float>(n);
        }

        case condition_subject::military_strength:
        {
            // BL-459: strength is DERIVED, not stored — unit_strength() applies
            // the roster's per-type quality and the unit's supply factor. It
            // returns an INTEGER (the x100 fixed point) on purpose, so this sum
            // stays integral and therefore order-independent; the loop reads an
            // unordered_map, and a float accumulator here would make it
            // nondeterministic (float addition is not associative).
            long long s = 0; // integer sum: order-independent by construction
            for (const auto& [uid, uc] : w.units)
                if (uc.owner == subject_corp)
                    s += unit_strength(w, uc);
            return static_cast<float>(s);
        }
    }
    return 0.0f;
}

bool evaluate_condition(const condition& c, const world& w, entity_id subject_corp)
{
    const float measured = measure_condition(c, w, subject_corp);

    // Integral subjects compare as integers, so `exactly 3` means three and not
    // "within an epsilon of three". Continuous subjects compare as floats —
    // rounding a stockpile of 3.5 units would be a lie about real state.
    if (condition_subject_is_integral(c.subject))
    {
        const long long m = std::llround(measured);
        const long long o = std::llround(c.operand);
        switch (c.comparator)
        {
            case condition_comparator::at_least:     return m >= o;
            case condition_comparator::greater_than: return m >  o;
            case condition_comparator::exactly:      return m == o;
            case condition_comparator::at_most:      return m <= o;
            case condition_comparator::less_than:    return m <  o;
        }
        return false;
    }

    switch (c.comparator)
    {
        case condition_comparator::at_least:     return measured >= c.operand;
        case condition_comparator::greater_than: return measured >  c.operand;
        case condition_comparator::exactly:      return measured == c.operand;
        case condition_comparator::at_most:      return measured <= c.operand;
        case condition_comparator::less_than:    return measured <  c.operand;
    }
    return false;
}

bool evaluate(const condition_set& cs, const world& w, entity_id subject_corp)
{
    // Property 2: an empty set is true, and it is true by falling out of the
    // loop rather than by a special case in front of it.
    for (const condition& c : cs.all)
        if (!evaluate_condition(c, w, subject_corp))
            return false;
    return true;
}

std::string condition_text(const condition& c,
                           const char* (*resource_label)(resource_type),
                           const char* (*structure_label)(building_type))
{
    const char* subject = "";
    switch (c.subject)
    {
        case condition_subject::research:          subject = "Research";          break;
        case condition_subject::structure:         subject = "Structures";        break;
        case condition_subject::stockpile:         subject = "Stockpile";         break;
        case condition_subject::market:            subject = "Market price";      break;
        case condition_subject::surplus:           subject = "Balance";           break;
        case condition_subject::era:               subject = "Era";               break;
        case condition_subject::military_units:    subject = "Units";             break;
        case condition_subject::military_strength: subject = "Military strength"; break;
    }

    const char* cmp = "";
    switch (c.comparator)
    {
        case condition_comparator::at_least:     cmp = ">="; break;
        case condition_comparator::greater_than: cmp = ">";  break;
        case condition_comparator::exactly:      cmp = "=";  break;
        case condition_comparator::at_most:      cmp = "<="; break;
        case condition_comparator::less_than:    cmp = "<";  break;
    }

    std::string qualifier;
    if (c.subject == condition_subject::research)
    {
        qualifier = " " + c.key;
    }
    else if (c.subject == condition_subject::stockpile || c.subject == condition_subject::market)
    {
        qualifier = resource_label
                    ? std::string(" of ") + resource_label(c.resource)
                    : " of resource #" + std::to_string(static_cast<int>(c.resource));
    }
    else if (c.subject == condition_subject::structure)
    {
        qualifier = structure_label
                    ? std::string(" of ") + structure_label(c.structure)
                    : " of type #" + std::to_string(static_cast<int>(c.structure));
    }

    // Whole numbers print without a decimal tail; a fractional threshold keeps
    // one place. Two authored thresholds that read the same must print the same.
    std::string operand;
    if (std::fabs(c.operand - std::round(c.operand)) < 0.0005f)
    {
        operand = std::to_string(static_cast<long long>(std::llround(c.operand)));
    }
    else
    {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%.1f", static_cast<double>(c.operand));
        operand = buf;
    }

    return std::string(subject) + qualifier + " " + cmp + " " + operand;
}
