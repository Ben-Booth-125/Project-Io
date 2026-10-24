#pragma once

// ---------------------------------------------------------------------------
// BL-1335 — TAXES AND DISCONTENT (docs/politics/NATIONS.md § Taxes and
// Discontent, Ben 2026-10-16). Header-only: the dials and the pure arithmetic.
// The world-reading halves live where their flows already are — the quarterly
// tax and the Discontent step in nation_step.cpp, the output penalty at the two
// points production output accrues (economy_system.cpp), the AI tier pick in
// nation_ai.cpp, the player's verb in corp_command.cpp.
//
// A nation taxes the WAGES its own buildings pay (BL-1403, Ben 2026-10-23: "Taxes
// are taken from wages, which are also paid for by the nation") at a tier I..V:
// each tick the tier's rate of those wages comes back to its treasury, so a higher
// tier lowers the net wage bill (`nation_wage_tax`, economy_system.hpp). It
// replaced the tax on home firms' operating profit, which had no firm left to
// tax. A tier above the middle
// raises the nation's Discontent each quarter, the middle holds it, a tier below
// lowers it; above the bite threshold every building on the nation's GROUND (any
// owner) loses output in proportion, up to `max_penalty` at full Discontent.
// ---------------------------------------------------------------------------

#include "entity.hpp"

#include <array>
#include <cmath>
#include <cstddef>

/// Tier bounds, and the tier every nation opens at — the MIDDLE tier, the one
/// that holds Discontent where it is. Structural, not a dial: "the middle tier
/// holds it" is the design, so the neutral tier is III by definition.
inline constexpr int k_tax_tier_min     = 1;
inline constexpr int k_tax_tier_max     = 5;
inline constexpr int k_default_tax_tier = 3;
inline constexpr int k_tax_tier_count   = 5;

/// The tax dials (economy.tax in scripts/economy.lua). Defaults are the shipped
/// values, so a registry that never read Lua runs the same mechanic.
struct tax_params
{
    /// The share of the wages it pays that the nation takes back, by tier I..V
    /// (BL-1403). Each finite in [0, 1].
    std::array<float, k_tax_tier_count> rate = {0.0f, 0.05f, 0.10f, 0.15f, 0.20f};

    /// Discontent added per quarter at each tier I..V (negative = ebbs). Each
    /// finite in [-1, 1]. Binary fractions so the quarterly sum is exact: tier IV
    /// reaches the 0.5 threshold in exactly 8 quarters, tier V in 4.
    std::array<float, k_tax_tier_count> pressure = {-0.125f, -0.0625f, 0.0f, 0.0625f, 0.125f};

    /// Discontent above which output is cut. In [0, 1].
    float bite_threshold = 0.5f;

    /// The output cut at Discontent 1. In [0, 1].
    float max_penalty = 0.15f;

    /// The AI tier scorer's margin under the threshold: it never steps to a
    /// tier whose one-cadence projection would carry Discontent past
    /// `bite_threshold - ai_headroom`, and steps down when the current tier's
    /// projection would. In [0, 1].
    float ai_headroom = 0.0625f;
};

inline bool tax_tier_valid(int tier)
{
    return tier >= k_tax_tier_min && tier <= k_tax_tier_max;
}

inline bool discontent_valid(float d)
{
    return std::isfinite(d) && d >= 0.0f && d <= 1.0f;
}

/// The tier's index into the per-tier arrays; the tier is clamped first so a
/// corrupt value can never index out of range.
inline std::size_t tax_tier_index(int tier)
{
    const int t = tier < k_tax_tier_min ? k_tax_tier_min : (tier > k_tax_tier_max ? k_tax_tier_max : tier);
    return static_cast<std::size_t>(t - k_tax_tier_min);
}

/// Discontent after one quarter at @p tier, clamped to [0, 1].
inline float discontent_after_quarter(float d, int tier, const tax_params& p)
{
    const float n = d + p.pressure[tax_tier_index(tier)];
    return n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n);
}

/// The output cut at Discontent @p d: 0 at or below the threshold, rising
/// linearly to `max_penalty` at 1.
inline float discontent_penalty(float d, const tax_params& p)
{
    if (!(d > p.bite_threshold) || !(p.bite_threshold < 1.0f))
        return 0.0f;
    const float x = (d - p.bite_threshold) / (1.0f - p.bite_threshold);
    return p.max_penalty * (x > 1.0f ? 1.0f : x);
}

/// What one nation's tax quarter did (reported, never stored): the tier it
/// taxed at, the credits it took from how many home firms, and Discontent
/// before and after the quarter's pressure. Ascending nation id.
struct nation_tax_row
{
    entity_id nation          = null_entity;
    int       tier            = k_default_tax_tier;
    float     collected       = 0.0f; ///< Credits into the treasury — the sum of the firms' debits.
    int       firms_taxed     = 0;    ///< Home firms with a positive operating profit this quarter.
    float     discontent_from = 0.0f;
    float     discontent_to   = 0.0f;
};

/// What the tax scorer weighed for one DUE nation (reported, never stored).
struct tax_ai_pick
{
    entity_id nation     = null_entity;
    int       tier_from  = k_default_tax_tier;
    int       tier_to    = k_default_tax_tier;
    bool      short_     = false; ///< The budget asked for more than the treasury funded this quarter.
    float     discontent = 0.0f;
};
