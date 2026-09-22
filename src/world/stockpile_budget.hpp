#pragma once

// ---------------------------------------------------------------------------
// stockpile_budget — BL-1042 (stockpile to budget). A region's industry points
// become its campaign centres' charter budgets.
//
// THE DESIGN IS DIGITISATION.md Part III (the downscale): "a region's points
// reach its campaign centres by the carve's slots, and a carved centre dropped
// when its body is built out takes its share unspent", with § Beat 1 (the stock
// sits on each region that holds centres; the treasury's share was already
// spread over a polity's regions by urban scale inside the sim, BL-1056) and
// § 1 (a centre's unspent stockpile at 1960 IS its charter budget, and that
// budget has ONE source — this).
//
// WHAT IT READS. `world::gen_settlement->regions[r].industry_points` (the sim's
// located stock) and the carve index `world::gen_carve_centres` /
// `world::gen_carve_dropped` (which centre is which region's k-th). Nothing
// else: no centre's scale, population or tile is read, so the split is the
// carve's own rank-size read and nothing new.
//
// THE SPLIT. Each region's points go over ALL its carved slots — founded and
// dropped — in proportion to the slot key (`urban_population / rank`), by
// largest-remainder apportionment: every slot takes the floor of its exact
// share, and what the floors leave goes one point each to the largest
// remainders, ties to the lower rank. Integer and exact; the parts sum to the
// region's points. A founded slot's share is its centre's budget; a DROPPED
// slot's share is counted unspent under its own reason, never handed to its
// siblings. Anchors and coverage foundings carry no slot and get nothing.
//
// EVERY POINT IS ACCOUNTED FOR: points_total == budget.total() + the unspent
// reasons, always — including on a rejection, where every point is `rejected`.
//
// THE PRICE IS THE STOCK'S OWN (BL-1064, NR-907): one firm charter costs the
// whole stockpile over `k_stockpile_price_divisor`, derived here once and carried
// on the budget, so the spend charges every world the same SHARE of itself.
//
// WITH THE DIGITISATION SPAN OFF (the legacy arc; it runs by default since
// BL-1044) no region holds a point, the budget is EMPTY, and an empty budget is
// the pre-budget world byte for byte (`apply_landscape_candidate`'s legacy
// branch runs first).
//
// NOTHING HERE IS PERSISTENT: a pure function of a generated world, built at
// new-game and spent there. No save field, no Lua key.
// ---------------------------------------------------------------------------

#include "charter_budget.hpp"
#include "entity.hpp"
#include "world.hpp"   // carve_slot, carve_dropped_slot (the carve index's types)

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct region;   // settlement.hpp

/// Why a region's industry point reached no centre's budget. Nothing here is
/// persistent, so the numbering follows the reading.
///
/// A region with points but NO CARVED CENTRE is split by cause (NR-901, Ben
/// 2026-09-19, option A: "points on a region whose towns were razed are lost
/// with them"). A region earns points only while it holds centres, so points on
/// a region the carve towns nobody mean its towns were lost after they built:
/// `razed` when history destroyed them (`region::centres_razed` > 0 — the cause
/// the map shows). `no_carved_centre` is the RESIDUAL: points on a region that
/// carved nothing yet records no razing. Under the ruling it should read ~0 —
/// the treasury no longer lands points on townless ground (NR-901's other half)
/// — so a non-zero residual is a finding, not a bucket.
enum class stockpile_unspent_reason : std::uint8_t
{
    carve_dropped    = 0, ///< its slot's carved centre was never founded: the body was built out
    carve_no_tile    = 1, ///< its slot's carved centre resolved to no tile (defensive; unreached)
    razed            = 2, ///< the region carved no centre because history razed its towns
                          ///< (`centres_razed` > 0): the works went with the towns (NR-901)
    no_carved_centre = 3, ///< RESIDUAL: the region carved no centre and records no razing
    rejected         = 4, ///< the whole budget was REJECTED (a domain violation or an
                          ///< inconsistent world; see `rejection`)
};

constexpr int stockpile_unspent_reason_count = 5;

inline const char* stockpile_unspent_reason_name(stockpile_unspent_reason r)
{
    switch (r)
    {
    case stockpile_unspent_reason::carve_dropped:    return "carve_dropped";
    case stockpile_unspent_reason::carve_no_tile:    return "carve_no_tile";
    case stockpile_unspent_reason::razed:            return "razed";
    case stockpile_unspent_reason::no_carved_centre: return "no_carved_centre";
    case stockpile_unspent_reason::rejected:         return "rejected";
    }
    return "?";
}

/// One region holding points: where they went.
struct stockpile_region_row
{
    int          region     = -1;
    std::int64_t points     = 0;  ///< the region's `industry_points`
    std::int64_t to_centres = 0;  ///< the shares its founded carved centres took
    int          founded    = 0;  ///< its carved slots that were founded
    int          dropped    = 0;  ///< its carved slots that were not
    std::array<std::int64_t, stockpile_unspent_reason_count> unspent{};
};

/// The stockpile, turned into a charter budget, and the account of every point.
struct stockpile_budget
{
    /// Points by CARVED centre id. Empty with the span off, and on a rejection.
    charter_budget budget;

    /// Set when a value left the domain below; the budget is then EMPTY (today's
    /// world) and every point is unspent as `rejected`. A rejection is decided
    /// before anything is written, and the builder writes nothing to the world
    /// either way. ONE CAVEAT, stated rather than hidden: a violation in the
    /// STOCK ITSELF (a region outside [0, industry_points_ceiling], or a total
    /// past 2^62) stops the read there, so `points_total` is what was summed up
    /// to it — such a stock cannot be accounted exactly, and the text says which.
    bool        rejected = false;
    std::string rejection;

    std::int64_t points_total      = 0; ///< every region's `industry_points`, summed
    std::int64_t points_to_centres = 0; ///< == budget.total()

    /// BL-1064 — THE FIRM PRICE, DERIVED (Ben, 2026-09-21, NR-907;
    /// DIGITISATION.md § 1): the world's WHOLE stockpile (`points_total`, every
    /// region's points, the ones no centre took included) divided by
    /// `price_divisor`, in whole points, and never below 1 — fixed here, once,
    /// when the budget is built. So the seat menu is about the same size on every
    /// world: a centre's points and the price both scale with the stock. 0 on
    /// every EMPTY budget — the span off, a stock whose every point went unspent
    /// (all razed or dropped), and a rejection — because an empty budget is
    /// today's world and prices nothing.
    std::int32_t firm_price_points = 0;
    /// The divisor the price was derived by (the build's argument); 0 wherever
    /// the price is.
    std::int64_t price_divisor     = 0;
    std::array<std::int64_t, stockpile_unspent_reason_count> unspent{};

    /// Every region with points > 0, ascending region index.
    std::vector<stockpile_region_row> regions;

    std::int64_t points_unspent() const
    {
        std::int64_t t = 0;
        for (const std::int64_t v : unspent)
            t += v;
        return t;
    }
    /// The account closes: every point went to a centre or has a reason.
    bool balanced() const { return points_total == points_to_centres + points_unspent()
                                && points_to_centres == budget.total(); }
};

// --- THE DOMAIN (reject, never clamp) ----------------------------------------

/// Most points the account can total and stay exact in int64: each region is at
/// most `industry_points_ceiling` (2^60), so adding one to a total at or under
/// this never overflows.
inline constexpr std::int64_t stockpile_points_total_max = 1LL << 62;
/// Largest slot key the split weighs (the sim's own per-region heads bound).
inline constexpr std::int64_t stockpile_slot_key_max = 1LL << 31;
/// Largest sum of one region's slot keys. With a key <= 2^31 the staged
/// apportionment's remainder product stays under 2^63.
inline constexpr std::int64_t stockpile_region_keys_max = 1LL << 32;

// --- THE PRICE (BL-1064) -------------------------------------------------------

/// The divisor a world's whole stockpile is split by into the price of ONE firm
/// charter (Ben, 2026-09-21, NR-907: "a charter's price is the world's whole
/// industry stockpile divided by a constant"; DIGITISATION.md § 1).
///
/// TWO KNOBS, TWO JOBS (Ben, 2026-09-21, NR-908; DIGITISATION.md § 1).
/// A centre affords a specialist when its points cover
/// `k_stockpile_specialist_firm_charters` / this of the world's stock, so the
/// SEAT MENU turns on the ratio of the two alone, and the specialist's price in
/// firm charters is the knob set against it. THIS divisor sets how many firm
/// charters a world's stock buys — its density, and so its tick — and is the
/// knob set against LIVE-PLAY COST.
///
/// PINNED AT 580 (Ben, 2026-09-21, NR-910). With the specialist at two firm
/// charters (below), 580 is the divisor at which the median library world opens
/// the seat-menu anchor's nine seats. Read off the seat curve
/// (`stockpile_budget_check --seat-curve`, 16 library seeds, m = 2): median
/// 6.5 / 7 / 8 / 9.5 / 12.5 seats at d = 520 / 540 / 560 / 580 / 600, and 580 is
/// the smallest divisor measured at which no library world opens none (seed 37:
/// 0 at 560, 2 at 580). A LARGER ratio d/m is a CHEAPER seat and MORE seats.
/// The spread is ACCEPTED, not capped — 2 to 73 seats across the library at
/// 580:2: the anchor is a median, and a world with many near-equal cities crosses
/// the price together. Live-play cost: the divisor alone sets the tick (BL-1043
/// stage 2: x0.43 / x0.91 / x1.70 the legacy world at 325:2 / 650:4 / 1300:8),
/// so 580 runs near x0.8 the legacy tick by interpolation; BL-1044's measurement
/// reads it on the shipped world.
inline constexpr std::int64_t k_stockpile_price_divisor = 580;
static_assert(k_stockpile_price_divisor > 0, "the price divisor must be > 0");

/// Build the charter budget from @p w's stockpile, its firm price derived by
/// @p price_divisor (the shipped constant unless an instrument names another).
/// Pure: reads the settlement record and the carve index, writes nothing. A
/// world with no settlement record (a loaded world, a fixture) or no points
/// returns an EMPTY budget with nothing to account and no price.
///
/// A world whose stock holds points while its carve index is EMPTY (both lists)
/// is INCONSISTENT — the index was lost (a copy or a load that kept the
/// settlement record but not the index) — and is REJECTED whole, so a lost
/// index can never pass as regions that carved no centre. So is a stock with
/// points priced by a divisor <= 0, or whose derived price is past int32 (the
/// spend's price type): rejected, never clamped.
stockpile_budget build_stockpile_budget(const world& w,
                                        std::int64_t price_divisor = k_stockpile_price_divisor);

/// The same builder over its three inputs, for a caller holding them apart from
/// a world (tools/verify/stockpile_budget_check's hand-built slots). @p regions
/// may be null (no settlement record: an empty budget).
stockpile_budget build_stockpile_budget(const std::vector<region>*            regions,
                                        const std::map<entity_id, carve_slot>& founded,
                                        const std::vector<carve_dropped_slot>& dropped,
                                        std::int64_t price_divisor = k_stockpile_price_divisor);

// --- THE SPEND ---------------------------------------------------------------
//
// THE SHIPPED SPEND'S NAMED CONSTANTS, SET HERE AND NOT IN charter_budget.hpp
// (whose prices have no shipped default by design). Every one is RULED
// (DIGITISATION.md § 1): the specialist's price anchored to the seat menu and the
// square root's base (Ben, 2026-09-21, NR-910), the density ceiling (NR-902).
// The FIRM price is not a constant at all: it is derived from the world's own
// stockpile by `k_stockpile_price_divisor` (above, NR-907). They are read only
// when the budget is non-empty — a world the Digitisation span ran on; with the
// span off no price is read.

/// Firm charters one specialist costs: TWO (Ben, 2026-09-21, NR-910). A
/// specialist's price is this many DERIVED firm prices (BL-1039's structure,
/// NR-907), and this is the knob anchored to the SEAT MENU (Ben, 2026-09-18 and
/// 2026-09-21, NR-908): with the divisor above, it sets the share of the stock a
/// seat costs, and the seats turn on the ratio d/m alone. Whole charters are too
/// coarse to land the anchor on their own — at the divisor that runs the legacy
/// tick, three open a median of about four seats and two about thirteen — so the
/// divisor takes the last step (above).
inline constexpr std::int32_t k_stockpile_specialist_firm_charters = 2;
/// The per-good cap's floor and the square root's base c: 8, Pass 6's legacy
/// per-good cap (Ben, 2026-09-21, NR-910), so a body at the legacy firm spend
/// keeps the legacy cap (`charter_sqrt_per_good_cap`: cap(B_ref) == c).
inline constexpr std::int32_t k_stockpile_per_resource_firm_cap = 8;
/// The anti-runaway guard per body (Pass 6's 200).
inline constexpr std::int32_t k_stockpile_max_firms_per_body = 200;
/// The density ceiling under the ruled square-root rule: 120 background firms
/// per body. RULED (Ben, 2026-09-19, NR-902; DIGITISATION.md § 1): on the cost
/// table's square-root rows the ceiling is what binds — at four times the
/// reference budget it trims every good evenly to 12 firms — while 160 never
/// bound (each good's own cap of 15 filled first) and cost a 12-31% dearer
/// economy tick for it. A ceiling that never binds is not a brake.
inline constexpr std::int32_t k_stockpile_density_ceiling = 120;

/// The spend @p sb is charged at: its own DERIVED firm price (BL-1064), the
/// ruled `sqrt_capital` cap rule under the constants above, window 4, the
/// province cap on (§ 1: "the per-province cap stays at 2 on a budget world").
/// On an empty or rejected budget the price is 0 — never read, since an empty
/// budget takes the legacy branch before any spend param is.
charter_spend_params stockpile_charter_spend(const stockpile_budget& sb);

// --- THE SEARCH-LESS PATHS (BL-1044, NR-909) ----------------------------------

class recipe_registry;

/// What `spend_stockpile_on_seed_candidate` did, for the caller's one line.
struct seed_candidate_spend
{
    stockpile_budget     stockpile;   ///< the world's own budget, as built
    charter_spend_report report;      ///< the apply's report; untouched when not spent
    bool                 spent = false;
};

/// NR-909 (Ben, 2026-09-21): --verify, --serve and the headless run never
/// search, and once the Digitisation span runs by default their worlds carry a
/// stockpile budget. Where the budget is NON-EMPTY this spends it as the
/// harness's unsearched apply does (`apply_shipped_landscape` with search =
/// false): the search's SEED CANDIDATE — placement seed `world_seed ^
/// 0x8A21F00D` (app.cpp's search seed), @p corporation_count, road tier 1 —
/// laid by `apply_landscape_candidate`'s budget overload at
/// `stockpile_charter_spend`, then the second recipe pass. So a refused or
/// no-specialist budget falls back inside world/* exactly as the app's does.
///
/// Where the budget is EMPTY (the span did not run, or the budget was
/// rejected) it touches NOTHING and returns `spent = false`: the caller lays
/// its own pre-budget web, byte for byte, as it did before this existed.
seed_candidate_spend spend_stockpile_on_seed_candidate(world& w, const recipe_registry& reg,
                                                       std::uint32_t world_seed,
                                                       int corporation_count);
