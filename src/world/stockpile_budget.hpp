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

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct world;

/// Why a region's industry point reached no centre's budget.
enum class stockpile_unspent_reason : std::uint8_t
{
    carve_dropped    = 0, ///< its slot's carved centre was never founded: the body was built out
    carve_no_tile    = 1, ///< its slot's carved centre resolved to no tile (defensive; unreached)
    no_carved_centre = 2, ///< the region held points but the carve gave it no centre at all
                          ///< (emptied or razed by the epoch, so it towns nobody)
    rejected         = 3, ///< the whole budget was REJECTED (a domain violation; see `rejection`)
};

constexpr int stockpile_unspent_reason_count = 4;

inline const char* stockpile_unspent_reason_name(stockpile_unspent_reason r)
{
    switch (r)
    {
    case stockpile_unspent_reason::carve_dropped:    return "carve_dropped";
    case stockpile_unspent_reason::carve_no_tile:    return "carve_no_tile";
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

/// Build the charter budget from @p w's stockpile. Pure: reads the settlement
/// record and the carve index, writes nothing. A world with no settlement
/// record (a loaded world, a fixture) or no points returns an EMPTY budget with
/// nothing to account.
stockpile_budget build_stockpile_budget(const world& w);

// --- THE SPEND ---------------------------------------------------------------
//
// PROVISIONAL NAMED CONSTANTS, SET HERE AND NOT IN charter_budget.hpp (whose
// prices have no shipped default by design). BL-1044 sets them against live-play
// cost and the seat menu (DIGITISATION.md § 1: the firm price "measured against
// live-play cost before it is fixed"; the specialist's price "anchored to the
// seat menu"; the square root's constants and the density ceiling "read off the
// cost table"). They are read only when the budget is non-empty, which needs the
// Digitisation span on; with it off (the shipped default) no price is read.

/// Points one background firm charter costs. PROVISIONAL (BL-1044).
inline constexpr std::int32_t k_stockpile_firm_price_points = 10000;
/// Firm charters one specialist costs. PROVISIONAL (BL-1044); the synthetic
/// budget's 4, the only reading the seat has been measured at.
inline constexpr std::int32_t k_stockpile_specialist_firm_charters = 4;
/// The per-good cap's floor and base c (Pass 6's 8, the legacy anchor).
inline constexpr std::int32_t k_stockpile_per_resource_firm_cap = 8;
/// The anti-runaway guard per body (Pass 6's 200).
inline constexpr std::int32_t k_stockpile_max_firms_per_body = 200;
/// The density ceiling under the ruled square-root rule. PROVISIONAL (BL-1044):
/// the top of the 81-150 band DIGITISATION.md § 1 names for the cost table.
inline constexpr std::int32_t k_stockpile_density_ceiling = 150;

/// The spend a stockpile budget is charged at: the ruled `sqrt_capital` cap rule
/// under the constants above, window 4, the province cap on (§ 1: "the
/// per-province cap stays at 2 on a budget world").
charter_spend_params stockpile_charter_spend();
