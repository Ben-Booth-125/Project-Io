#pragma once

// ---------------------------------------------------------------------------
// charter_budget — BL-1032. A per-centre charter budget, and what spending it did.
//
// THE DESIGN IS DIGITISATION.md § 1 (the budget charters the whole web; one
// specialist per centre that affords one, richest first; nation balance
// dropped; a charter stays near its centre; no budget and an empty budget are
// today's world) and CORPORATION_GENERATION.md Pass 1 / Pass 6. This file is
// the TYPES only; the spend is `charter_web_from_budget`
// (corporation_generation.hpp) and the one branch that reaches it is the
// budget overload of `apply_landscape_candidate` (landscape_search.hpp).
//
// NOTHING HERE IS PERSISTENT. A budget is an input to generation and a report
// is a reading of one generation; neither is a world field, neither enters
// `write_world_snapshot` or `state_hash`, and neither has a Lua key. The budget
// has ONE source by design — a centre's unspent industry-point stockpile — and
// nothing in src/ builds one; a caller that has no stockpile passes none.
// ---------------------------------------------------------------------------

#include "entity.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

/// Charter points by POPULATION-CENTRE entity id, ascending.
///
/// ENTRIES <= 0 ARE DROPPED ON CONSTRUCTION, so an empty budget and a budget of
/// all-zero entries are ONE state — `empty()` — and both take the legacy branch.
/// There is no mutator that could put a non-positive entry back: `set` erases
/// on <= 0 for the same reason.
class charter_budget
{
public:
    charter_budget() = default;

    explicit charter_budget(const std::map<entity_id, std::int32_t>& points)
    {
        for (const auto& [centre, pts] : points)
            if (pts > 0)
                m_points.emplace(centre, pts);
    }

    /// Set @p centre's points; a value <= 0 removes the entry.
    void set(entity_id centre, std::int32_t pts)
    {
        if (pts > 0)
            m_points[centre] = pts;
        else
            m_points.erase(centre);
    }

    /// True for no entries — which is also the all-zero budget (see the class note).
    bool empty() const { return m_points.empty(); }

    const std::map<entity_id, std::int32_t>& points() const { return m_points; }

    /// Sum of every entry, widened so a large budget cannot overflow the total.
    std::int64_t total() const
    {
        std::int64_t t = 0;
        for (const auto& kv : m_points)
            t += kv.second;
        return t;
    }

private:
    std::map<entity_id, std::int32_t> m_points;
};

/// How a budget is spent. THE PRICES HAVE NO SHIPPED DEFAULT (DIGITISATION.md
/// § 1: "the price of a specialist and of a firm are measured against live-play
/// cost before either is fixed"). They default to 0, and a NON-EMPTY budget with
/// a price <= 0 is REFUSED (`charter_spend_refusal`): the search declines to
/// run, and the spend charters nothing and reports every point unspent. An
/// empty budget is never refused — it is today's world, whatever the prices.
struct charter_spend_params
{
    /// Points one background firm costs. Must be > 0 on a non-empty budget.
    std::int32_t firm_price_points = 0;
    /// Points one specialist costs. Must be > 0 on a non-empty budget.
    std::int32_t specialist_price_points = 0;
    /// The anchor window's radius around the centre tile, in grid tiles, on a
    /// column-WRAPPED squared metric (dx*dx + dy*dy <= r*r). 4 is the seat's
    /// `population_radius` precedent.
    int window_radius = 4;
    /// Pass 6's per-province firm cap (2) applies to budget firms when true.
    bool province_cap = true;
};

/// Why a point was not spent. Ordered: a report sorts (centre, reason) on it.
enum class charter_unspent_reason : std::uint8_t
{
    no_nation        = 0, ///< the centre's tile belongs to no nation (or has no tile)
    window_exhausted = 1, ///< no anchorable ground in the centre window nor its region window
    no_gap           = 2, ///< the body had no resource short enough to charter a firm for
    body_cap         = 3, ///< the body already carries Pass 6's 200 background firms
    remainder        = 4, ///< fewer points left than one firm costs
    refused          = 5, ///< the spend params were refused (a price <= 0); nothing chartered
};

inline const char* charter_unspent_reason_name(charter_unspent_reason r)
{
    switch (r)
    {
    case charter_unspent_reason::no_nation:        return "no_nation";
    case charter_unspent_reason::window_exhausted: return "window_exhausted";
    case charter_unspent_reason::no_gap:           return "no_gap";
    case charter_unspent_reason::body_cap:         return "body_cap";
    case charter_unspent_reason::remainder:        return "remainder";
    case charter_unspent_reason::refused:          return "refused";
    }
    return "?";
}

constexpr int charter_unspent_reason_count = 6;

/// Which anchor rung a charter landed on. There is no third rung: a charter that
/// finds no ground in either is UNSPENT, never scattered nation-wide.
enum class charter_rung : std::uint8_t
{
    centre_window = 1, ///< the centre nation's tiles within `window_radius` of the centre tile
    region_window = 2, ///< the centre nation's tiles in the centre's region
};

struct charter_unspent
{
    entity_id              centre = null_entity;
    charter_unspent_reason reason = charter_unspent_reason::remainder;
    std::int32_t           points = 0;
};

struct charter_record
{
    entity_id    corp        = null_entity;
    entity_id    centre      = null_entity;
    bool         specialist  = false;
    charter_rung rung        = charter_rung::centre_window;
    entity_id    anchor_tile = null_entity; ///< the tile the first holding stands on
    std::int32_t price       = 0;
};

/// What one spend did. Every vector is SORTED — ids ascending, unspent by
/// (centre, reason) — so a report prints and compares the same on every run.
struct charter_spend_report
{
    /// Set when `charter_spend_refusal` refused the params; `refusal` says why.
    bool        refused = false;
    std::string refusal;

    std::vector<entity_id>       specialists;  ///< ascending id
    std::vector<entity_id>       firms;        ///< ascending id
    std::vector<charter_record>  charters;     ///< ascending corp id
    std::vector<charter_unspent> unspent;      ///< ascending (centre, reason); zero rows omitted

    /// The seeded pick among budget specialists, or null.
    entity_id player = null_entity;
    /// No centre chartered a specialist, so nobody was picked — and nothing
    /// forces one (Ben, 2026-09-17: decided at sprint 45).
    bool no_specialists = false;

    std::int64_t points_budgeted = 0;
    std::int64_t points_spent    = 0;
    std::int64_t points_unspent  = 0;  ///< always points_budgeted - points_spent
};

/// Null when @p s may spend @p b; otherwise why not. An EMPTY budget is never
/// refused: it is today's world, and it takes the legacy branch before any
/// price is read.
inline const char* charter_spend_refusal(const charter_budget& b, const charter_spend_params& s)
{
    if (b.empty())
        return nullptr;
    if (s.firm_price_points <= 0)
        return "firm_price_points must be > 0 on a non-empty charter budget (no shipped default)";
    if (s.specialist_price_points <= 0)
        return "specialist_price_points must be > 0 on a non-empty charter budget (no shipped default)";
    if (s.window_radius < 0)
        return "window_radius must be >= 0";
    return nullptr;
}
