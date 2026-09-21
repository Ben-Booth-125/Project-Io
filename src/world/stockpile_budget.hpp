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
// WITH THE DIGITISATION SPAN OFF (the shipped default) no region holds a point,
// the budget is EMPTY, and an empty budget is today's world byte for byte
// (`apply_landscape_candidate`'s legacy branch runs first). That is the whole
// of the shipped behaviour until BL-1044 turns the span on.
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
/// industry stockpile divided by a constant"; DIGITISATION.md § 1). PROVISIONAL:
/// BL-1044 pins it off BL-1043 stage 2.
///
/// TWO KNOBS, TWO JOBS (Ben, 2026-09-21, NR-908; DIGITISATION.md § 1).
/// A centre affords a specialist when its points cover
/// `k_stockpile_specialist_firm_charters` / this of the world's stock, so the
/// SEAT MENU turns on the ratio of the two alone, and the specialist's price in
/// firm charters is the knob set against it. THIS divisor sets how many firm
/// charters a world's stock buys — its density, and so its tick — and is the
/// knob set against LIVE-PLAY COST.
///
/// WHERE 650 COMES FROM (stage 1, runs.real_stockpile_bl1043_a/_b/_c, 16 seeds x
/// firm price 10000/20000/40000 x 4 firm charters a specialist): a world's seats
/// run close to 0.056 x its stock / the specialist's price (the median of
/// seats x firm price / stock over the 48 rows is 0.01395, at 4 charters), so
/// under a derived price the seats are about 0.056 x this / the charters, and
/// the anchor's 9 seats is a ratio near 160 — 650 at 4 charters. A LARGER ratio
/// is a CHEAPER seat and MORE seats. Stage 2 reads the density at that ratio
/// along the divisor.
inline constexpr std::int64_t k_stockpile_price_divisor = 650;
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
// PROVISIONAL NAMED CONSTANTS, SET HERE AND NOT IN charter_budget.hpp (whose
// prices have no shipped default by design). BL-1044 sets them against live-play
// cost and the seat menu (DIGITISATION.md § 1: the specialist's price "anchored
// to the seat menu"; the square root's constants and the density ceiling "read
// off the cost table"). The density ceiling is the one already ruled (NR-902,
// below). The FIRM price is not a constant at all: it is derived from the
// world's own stockpile by `k_stockpile_price_divisor` (above, NR-907). They
// are read only when the budget is non-empty, which needs the Digitisation span
// on; with it off (the shipped default) no price is read.

/// Firm charters one specialist costs. PROVISIONAL (BL-1044); the synthetic
/// budget's 4, the only reading the seat has been measured at. A specialist's
/// price is this many DERIVED firm prices (BL-1039's structure, NR-907), and
/// this is the knob anchored to the SEAT MENU (Ben, 2026-09-18 and 2026-09-21):
/// with the divisor above, it sets the share of the stock a seat costs.
inline constexpr std::int32_t k_stockpile_specialist_firm_charters = 4;
/// The per-good cap's floor and base c (Pass 6's 8, the legacy anchor).
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
