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
// ONE builder: `build_stockpile_budget` (stockpile_budget.hpp, BL-1042), which
// the new-game path (app::start_new_game_prelude, mirrored by
// tools/verify/harness_params.hpp) passes to the search and the winner's apply.
// With the Digitisation span off that budget is empty, which is today's world.
// The prices it is charged at are named there, never defaulted here.
// ---------------------------------------------------------------------------

#include "entity.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
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

/// How the budget path caps background firms per good on a body (BL-1039).
///
/// The two LEGACY rules are BL-1033's measurement switch, kept selectable so
/// every reading taken on them reproduces: `fixed` is the old `resource_cap =
/// true` (cap kept), `lifted` the old `resource_cap = false` (cap lifted).
/// `sqrt_capital` is the ruled rule (DIGITISATION.md § 1, Ben 2026-09-18: "the
/// cap scales by a SQUARE ROOT of the body's charter capital, under a named
/// DENSITY CEILING"); see `charter_sqrt_per_good_cap` for the formula and what
/// B, B_ref and G are.
///
/// THE FILL ORDER IS A PROPERTY OF THE RULE (Ben, 2026-09-18: "fill goods in
/// turn: a firm per good each pass, up to its cap; the ceiling trims every good
/// evenly"). Under `sqrt_capital` background firms ALWAYS go to the goods IN
/// TURN — whether or not the ceiling ends up binding, because that is not known
/// until the walk ends and an order that switched on it would be neither
/// legible nor stable. The two legacy rules keep Pass 6's biggest-gap-first
/// order verbatim; their worlds are pinned. See `charter_web_from_budget`.
enum class charter_cap_rule : std::uint8_t
{
    fixed        = 0, ///< `per_resource_firm_cap` firms per good per body, flat; biggest gap first
    lifted       = 1, ///< no per-good cap (the construction yard's count bound still applies);
                      ///< biggest gap first
    sqrt_capital = 2, ///< max(cap, floor(cap x sqrt(B / B_ref))), under `density_ceiling`;
                      ///< goods filled IN TURN
};

inline const char* charter_cap_rule_name(charter_cap_rule r)
{
    switch (r)
    {
    case charter_cap_rule::fixed:        return "fixed";
    case charter_cap_rule::lifted:       return "lifted";
    case charter_cap_rule::sqrt_capital: return "sqrt_capital";
    }
    return "?";
}

/// How a budget is spent. THE PRICES HAVE NO SHIPPED DEFAULT (DIGITISATION.md
/// § 1: "the price of a specialist and of a firm are measured against live-play
/// cost before either is fixed"). They default to 0, and a NON-EMPTY budget with
/// a price <= 0 is REFUSED (`charter_spend_refusal`). A REFUSAL MUTATES NOTHING
/// BEYOND TODAY'S WORLD: it is checked before any mutation, the search then runs
/// exactly the no-budget search and flags the refusal, and the apply runs exactly
/// the legacy calls and reports every point unspent as `refused`. An empty budget
/// is never refused — it is today's world, whatever the prices.
///
/// THE SAME CONTRACT COVERS THE CAPS (BL-1039). The body guard, the per-good cap
/// and the density ceiling are the budget path's OWN numbers — no longer
/// restated from Pass 6, whose constants they do not follow — and none has a
/// shipped default: each is 0 until a caller sets it, and a non-empty budget
/// whose rule READS a number <= 0 is refused. A number set under a rule that
/// does NOT read it (a ceiling under a legacy cap rule, a per-good cap under
/// `lifted`) is refused too, so no row can carry a setting that silently did
/// nothing.
///
/// A BUDGET SPECIALIST'S CAPITAL IS TODAY'S DRAW (Ben, 2026-09-18): 400 +/- 40%
/// with the processing/trade premium, CORPORATION_GENERATION.md Pass 4 as
/// written. There is no capital parameter here.
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
    /// The budget path's per-province firm cap applies to budget firms when true.
    bool province_cap = true;

    /// THE PER-GOOD CAP RULE. `fixed` and `lifted` are BL-1033's cap kept and
    /// cap lifted (Ben, NR-889: measure both before ruling), read only by
    /// `charter_web_from_budget` — `generate_background_firms` keeps Pass 6's own
    /// cap whatever this says. Under every rule the construction-yard
    /// provisioning bound, the province cap switch, the body guard and the
    /// `no_gap` stop still apply.
    charter_cap_rule resource_cap_rule = charter_cap_rule::fixed;
    /// Firms per good per body: the flat cap under `fixed`, and the floor AND the
    /// base of the square root under `sqrt_capital`. Must be > 0 on a non-empty
    /// budget under `fixed` or `sqrt_capital`; unread under `lifted`, so it must
    /// be 0 there (refused otherwise) and a lifted body reports no B_ref.
    std::int32_t per_resource_firm_cap = 0;
    /// The ANTI-RUNAWAY guard: background firms a body may carry, counted as
    /// `body_cap`. Must be > 0 on a non-empty budget.
    std::int32_t max_firms_per_body = 0;
    /// THE DENSITY CEILING (DIGITISATION.md § 1): background firms a body may
    /// carry under `sqrt_capital`, counted as `density_ceiling` — BELOW the
    /// guard, so the guard goes back to catching runaways only. Under
    /// `sqrt_capital` it must satisfy 0 < ceiling < `max_firms_per_body`; under
    /// a legacy rule it must be 0 (they carry no ceiling).
    std::int32_t density_ceiling = 0;

    /// Points one specialist costs: the firm price times the firm charters it is
    /// worth. Widened, so no pair of int32 inputs overflows it; a price above any
    /// int32 budget entry is simply never affordable.
    std::int64_t specialist_price_points() const
    {
        return static_cast<std::int64_t>(firm_price_points)
             * static_cast<std::int64_t>(specialist_firm_charters);
    }
};

/// floor(sqrt(@p x)) for x >= 0, in integers — exact on every input, so the
/// per-good cap cannot sit on the wrong side of an integer boundary through
/// floating-point rounding. 0 for x <= 0.
inline std::int64_t charter_isqrt(std::int64_t x)
{
    if (x <= 0)
        return 0;
    std::int64_t lo = 0, hi = 3037000499LL;   // floor(sqrt(INT64_MAX))
    while (lo < hi)
    {
        const std::int64_t mid = lo + (hi - lo + 1) / 2;
        if (mid <= x / mid)
            lo = mid;
        else
            hi = mid - 1;
    }
    return lo;
}

/// THE SQUARE-ROOT RULE (BL-1039; DIGITISATION.md § 1, Ben 2026-09-18):
///
///     per-good cap = max(c, floor(c x sqrt(B / B_ref)))
///     B_ref        = c x |G| x firm_price_points
///
/// B AND B_REF ARE IN THE SAME UNITS — points spent on FIRMS (Ben, 2026-09-18):
///
/// c  = `per_resource_firm_cap` (8 in every measured row) — the legacy cap is
///      both the floor and the base.
/// B  = the body's points for FIRMS: over every centre on the body whose tile a
///      nation owns (a `no_nation` centre's points can buy nothing), its points
///      NET OF THE SPECIALIST PRICE where it affords one, in whole firm
///      charters — `charter_centre_firm_points`. Known before the walk: a centre
///      that affords a specialist sets its price aside whether or not the
///      specialist then finds ground, exactly as the walk does.
/// G  = the GOODS WITH DEMAND on the body — every good whose demand, all three
///      halves the gap selection reads (consumer + building upkeep +
///      construction), is > 0, measured ONCE before the walk. See
///      `charter_web_from_budget` for why it is fixed there.
/// B_ref = the points the legacy cap spends on firms on that body: c firms on
///      each demanded good, at the firm price.
///
/// SO cap(B_ref) == c EXACTLY: at B = B_ref = c x |G| x fp the integer below is
/// c x (c |G| fp) / (|G| fp) = c^2 with no remainder, isqrt(c^2) = c, and
/// max(c, c) = c. A body whose firm spend is the legacy firm spend keeps the
/// legacy cap; only a body that spends MORE on firms earns more per good
/// (charter_refusal_probe proves it over a grid of c, |G| and firm prices).
///
/// Exact integer arithmetic: floor(sqrt(y)) = isqrt(floor(y)) for y >= 0, and
/// c^2 x B / B_ref = c x B / (|G| x fp). A body with no demanded good (|G| = 0)
/// has no reference and keeps c. A cap too large for int32 — or a c x B past
/// int64 — saturates at int32's maximum: any cap at or above the density ceiling
/// already binds nothing.
inline std::int32_t charter_sqrt_per_good_cap(std::int32_t c, std::int64_t firm_points,
                                              int goods_with_demand, std::int32_t firm_price_points)
{
    if (c <= 0 || goods_with_demand <= 0 || firm_price_points <= 0 || firm_points <= 0)
        return c;
    const std::int64_t denom = static_cast<std::int64_t>(goods_with_demand)
                             * static_cast<std::int64_t>(firm_price_points);
    constexpr std::int64_t k_max = std::numeric_limits<std::int64_t>::max();
    if (firm_points > k_max / c)
        return std::numeric_limits<std::int32_t>::max();
    const std::int64_t y    = (static_cast<std::int64_t>(c) * firm_points) / denom;
    const std::int64_t root = charter_isqrt(y);
    const std::int64_t cap  = std::max<std::int64_t>(c, root);
    return static_cast<std::int32_t>(
        std::min<std::int64_t>(cap, std::numeric_limits<std::int32_t>::max()));
}

/// B's per-centre term (see `charter_sqrt_per_good_cap`): @p points NET OF THE
/// SPECIALIST PRICE when they afford it, rounded down to whole firm charters and
/// expressed in points. What the walk sets aside for firms at this centre
/// (`firm_points`), less the remainder no firm can be bought with. 0 when the
/// firm price is not positive (such a spend is refused anyway).
inline std::int64_t charter_centre_firm_points(std::int32_t points, const charter_spend_params& s)
{
    const std::int64_t fp = s.firm_price_points;
    if (fp <= 0 || points <= 0)
        return 0;
    std::int64_t left = points;
    const std::int64_t sp = s.specialist_price_points();
    if (sp > 0 && left >= sp)
        left -= sp;
    return (left / fp) * fp;
}

/// Why a point was not spent. Ordered: a report sorts (centre, reason) on it.
/// Nothing here is persistent, so the numbering follows the reading —
/// `density_ceiling` is appended rather than slotted beside `body_cap`, so every
/// reason BL-1033's table already carries keeps its column.
enum class charter_unspent_reason : std::uint8_t
{
    no_nation        = 0, ///< the centre's tile belongs to no nation (or has no tile)
    window_exhausted = 1, ///< no anchorable ground in the centre window nor its region window
    province_cap     = 2, ///< the windows HAD anchorable ground, but every such tile stands in
                          ///< a province already at the budget path's per-province firm cap
    no_gap           = 3, ///< the body had no resource short enough to charter a firm for
    body_cap         = 4, ///< the body already carries `max_firms_per_body` background firms
                          ///< (the anti-runaway guard)
    remainder        = 5, ///< fewer points left than one firm costs
    refused          = 6, ///< the spend params were refused (a price <= 0); nothing chartered
    density_ceiling  = 7, ///< the body already carries `density_ceiling` background firms
                          ///< (sqrt_capital only; BL-1039)
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
    case charter_unspent_reason::density_ceiling:  return "density_ceiling";
    }
    return "?";
}

constexpr int charter_unspent_reason_count = 8;

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

    /// The good a FIRM was chartered for (a resource index, as the selection —
    /// biggest gap first, or in turn under `sqrt_capital` — picked it); 0xFFFF on
    /// a specialist.
    std::uint16_t good = 0xFFFF;
};

/// One body's density rule, fixed before the walk, and what the walk put on it
/// (BL-1039). Every body holding a nation-resolved budgeted centre has a row.
struct charter_body_record
{
    entity_id    body = null_entity;
    /// B: the body's points for FIRMS — over its nation-resolved centres, net
    /// of each affordable specialist price, in whole firm charters
    /// (`charter_centre_firm_points`). The same units as B_ref.
    std::int64_t firm_points = 0;
    /// |G|: goods with demand on the body before the walk (see
    /// `charter_sqrt_per_good_cap`), and which they are (resource indices, ascending).
    int                        goods_with_demand = 0;
    std::vector<std::uint16_t> goods;
    /// B_ref = c x |G| x firm price (0 when |G| is 0, and under `lifted`, which
    /// carries no c). Reported under `fixed` and `sqrt_capital`;
    /// read only by `sqrt_capital`.
    std::int64_t reference_points = 0;
    /// The per-good cap in force on this body: c under `fixed`, the rule's under
    /// `sqrt_capital`, -1 under `lifted` (no cap).
    std::int32_t per_good_cap = -1;
    /// The density ceiling in force (0: none — every legacy rule).
    std::int32_t density_ceiling = 0;
    /// Background firms the walk chartered on this body, in all and per good
    /// (indexed by resource; the good each was chartered FOR, as the selection
    /// picked it).
    std::int32_t firms = 0;
    std::vector<std::int32_t> firms_by_good;   ///< size resource_count
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

    /// BL-1039: the cap rule the spend ran on, and each body's rule and firms
    /// per good (ascending body id).
    charter_cap_rule                 cap_rule = charter_cap_rule::fixed;
    std::vector<charter_body_record> bodies;
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
    // BL-1039 — the caps: the budget path's own numbers, no shipped default,
    // refused where read and <= 0, refused where set and unread.
    if (s.max_firms_per_body <= 0)
        return "max_firms_per_body must be > 0 on a non-empty charter budget (no shipped default)";
    switch (s.resource_cap_rule)
    {
    case charter_cap_rule::fixed:
    case charter_cap_rule::lifted:
        if (s.resource_cap_rule == charter_cap_rule::fixed && s.per_resource_firm_cap <= 0)
            return "per_resource_firm_cap must be > 0 under the fixed cap rule (no shipped default)";
        // Unread under `lifted` (no per-good cap), so a value there is refused:
        // a lifted row must not carry a c it never applied, nor report a B_ref
        // built from one (BL-1039 fix round).
        if (s.resource_cap_rule == charter_cap_rule::lifted && s.per_resource_firm_cap != 0)
            return "per_resource_firm_cap is not read under the lifted cap rule; a lifted spend with "
                   "a cap set would silently ignore it";
        if (s.density_ceiling != 0)
            return "density_ceiling is read only by the sqrt_capital cap rule; a legacy rule with a "
                   "ceiling set would silently ignore it";
        break;
    case charter_cap_rule::sqrt_capital:
        if (s.per_resource_firm_cap <= 0)
            return "per_resource_firm_cap must be > 0 under the sqrt_capital cap rule (no shipped default)";
        if (s.density_ceiling <= 0)
            return "density_ceiling must be > 0 under the sqrt_capital cap rule (no shipped default)";
        if (s.density_ceiling >= s.max_firms_per_body)
            return "density_ceiling must sit below max_firms_per_body (the runaway guard)";
        break;
    default:
        return "resource_cap_rule is not a known rule";
    }
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
