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
/// a price <= 0 is REFUSED (`charter_spend_refusal`). A REFUSAL MUTATES NOTHING
/// BEYOND TODAY'S WORLD: it is checked before any mutation, the search then runs
/// exactly the no-budget search and flags the refusal, and the apply runs exactly
/// the legacy calls and reports every point unspent as `refused`. An empty budget
/// is never refused — it is today's world, whatever the prices.
///
/// A SPECIALIST'S PRICE IS A WHOLE NUMBER OF FIRM CHARTERS (DIGITISATION.md § 1:
/// "a specialist's price — a fixed number of firm charters"), so it is not a free
/// number: it is `firm_price_points x specialist_firm_charters`.
struct charter_spend_params
{
    /// Points one background firm costs. Must be > 0 on a non-empty budget.
    std::int32_t firm_price_points = 0;
    /// How many firm charters one specialist costs. Must be > 0 on a non-empty
    /// budget.
    std::int32_t specialist_firm_charters = 0;
    /// The anchor window's radius around the centre tile, in grid tiles, on a
    /// column-WRAPPED squared metric (dx*dx + dy*dy <= r*r). 4 is the seat's
    /// `population_radius` precedent.
    int window_radius = 4;
    /// Pass 6's per-province firm cap (2) applies to budget firms when true.
    bool province_cap = true;
    /// Pass 6's per-RESOURCE firm cap (8) applies to the budget path's gap
    /// selection when true — the copy inside `charter_web_from_budget`, and
    /// nothing else: `generate_background_firms` keeps its own cap whatever this
    /// says. BL-1033 (Ben, NR-889: measure the cap KEPT and LIFTED before ruling
    /// which gives way) — false lifts it so a denser budget can buy past it; the
    /// construction-yard provisioning bound, the province cap switch, the
    /// 200-per-body cap and the `no_gap` stop all still apply.
    bool resource_cap = true;

    /// Points one specialist costs: the firm price times the firm charters it is
    /// worth. Widened, so no pair of int32 inputs overflows it; a price above any
    /// int32 budget entry is simply never affordable.
    std::int64_t specialist_price_points() const
    {
        return static_cast<std::int64_t>(firm_price_points)
             * static_cast<std::int64_t>(specialist_firm_charters);
    }
};

/// Why a point was not spent. Ordered: a report sorts (centre, reason) on it.
/// Nothing here is persistent, so the numbering follows the reading.
enum class charter_unspent_reason : std::uint8_t
{
    no_nation        = 0, ///< the centre's tile belongs to no nation (or has no tile)
    window_exhausted = 1, ///< no anchorable ground in the centre window nor its region window
    province_cap     = 2, ///< the windows HAD anchorable ground, but every such tile stands in
                          ///< a province already at Pass 6's per-province firm cap
    no_gap           = 3, ///< the body had no resource short enough to charter a firm for
    body_cap         = 4, ///< the body already carries Pass 6's 200 background firms
    remainder        = 5, ///< fewer points left than one firm costs
    refused          = 6, ///< the spend params were refused (a price <= 0); nothing chartered
};

inline const char* charter_unspent_reason_name(charter_unspent_reason r)
{
    switch (r)
    {
    case charter_unspent_reason::no_nation:        return "no_nation";
    case charter_unspent_reason::window_exhausted: return "window_exhausted";
    case charter_unspent_reason::province_cap:     return "province_cap";
    case charter_unspent_reason::no_gap:           return "no_gap";
    case charter_unspent_reason::body_cap:         return "body_cap";
    case charter_unspent_reason::remainder:        return "remainder";
    case charter_unspent_reason::refused:          return "refused";
    }
    return "?";
}

constexpr int charter_unspent_reason_count = 7;

/// Which anchor rung a charter landed on. There is no third rung: a charter that
/// finds no ground in either is UNSPENT, never scattered nation-wide.
enum class charter_rung : std::uint8_t
{
    centre_window = 1, ///< the centre nation's tiles within `window_radius` of the centre tile
    region_window = 2, ///< the centre nation's tiles in the centre's NEAREST region, and
                       ///< only when that region is the centre nation's own
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
    /// EVERY holding's tile as placed, the anchor first. The rung bounds only
    /// the anchor: `place_starting_assets` walks the secondary holdings outward
    /// from it across the whole nation, so a reader measures the spill here.
    std::vector<entity_id> holdings;
};

/// What one spend did. Every vector is SORTED — ids ascending, unspent by
/// (centre, reason) — so a report prints and compares the same on every run.
struct charter_spend_report
{
    /// Set when `charter_spend_refusal` refused the params; `refusal` says why.
    /// On `apply_landscape_candidate`'s budget overload a refused world is
    /// TODAY'S world — the legacy calls ran, and nothing here was chartered.
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
/// price is read. Every caller checks this BEFORE ANY MUTATION.
inline const char* charter_spend_refusal(const charter_budget& b, const charter_spend_params& s)
{
    if (b.empty())
        return nullptr;
    if (s.firm_price_points <= 0)
        return "firm_price_points must be > 0 on a non-empty charter budget (no shipped default)";
    if (s.specialist_firm_charters <= 0)
        return "specialist_firm_charters must be > 0 on a non-empty charter budget (no shipped default)";
    if (s.window_radius < 0)
        return "window_radius must be >= 0";
    return nullptr;
}

/// The report a REFUSED spend leaves: nothing chartered, nobody picked, every
/// point unspent with reason `refused`, and the refusal's text. It reads the
/// budget only — a refusal is decided before any world is touched.
inline charter_spend_report charter_refused_report(const charter_budget& b, const char* why)
{
    charter_spend_report rep;
    rep.refused         = true;
    rep.refusal         = (why != nullptr) ? why : "";
    rep.no_specialists  = true;
    rep.points_budgeted = b.total();
    for (const auto& [centre, pts] : b.points())
        rep.unspent.push_back({ centre, charter_unspent_reason::refused, pts });
    rep.points_unspent  = rep.points_budgeted;
    return rep;
}
