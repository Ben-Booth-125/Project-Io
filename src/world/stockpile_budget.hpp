#pragma once

// ---------------------------------------------------------------------------
// stockpile_budget — BL-1042 (stockpile to budget). A region's industry points
// become its campaign centres' charter budgets.
//
// THE DESIGN IS INDUSTRIALISATION.md Part III (the downscale): "a region's points
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
// BL-1168 (Ben, 2026-10-03) narrows WHOSE stock: a centre's charters are priced
// by the stock within its TRADE REACH — its landmass — over the same divisor
// (`charter_price_reach`), so a far landmass with little capital charters its
// own firms rather than being priced out by a richer continent's stock.
//
// WITH THE INDUSTRIALISATION SPAN OFF (the legacy arc; it runs by default since
// BL-1044) no region holds a point, the budget is EMPTY, and an empty budget is
// the pre-budget world byte for byte (`apply_landscape_candidate`'s legacy
// branch runs first).
//
// NOTHING HERE IS PERSISTENT: a pure function of a generated world, built at
// new-game and spent there. No save field, no Lua key.
// ---------------------------------------------------------------------------

#include "charter_budget.hpp"
#include "charter_price.hpp"  // k_stockpile_price_divisor, charter_running_price (BL-1099)
#include "entity.hpp"
#include "world.hpp"   // carve_slot, carve_dropped_slot (the carve index's types)

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct region;   // settlement.hpp

/// BL-1168 — WHOSE STOCK A CENTRE'S CHARTERS ARE PRICED BY (Ben, 2026-10-03;
/// INDUSTRIALISATION.md § Industry spreads beyond its heartland: "a centre's
/// charters are priced as a share of the stock within its trade reach -- its
/// landmass, or its market's catchment -- not of the whole world's").
///
/// A centre's REACH is its carve region's: the region's anchor decides it, and
/// every point of the stock is counted in the reach of the region that holds it
/// (the razed and dropped points included, as NR-907's world-wide price counts
/// them). The firm price at a centre is then its reach's stock over the same
/// divisor, floored and never below 1. A reach's stock is never more than the
/// world's, so no reach price exceeds the world price.
///   * `world`    — one price, the world's (NR-907; the pre-BL-1168 rule, kept
///                  selectable so its readings reproduce).
///   * `landmass` — THE SHIPPED READING: the region anchor's `landmass_at` over
///                  `landmass_labels` of the home body's tile substrate, the
///                  labels the span's own cross-water reads use.
///   * `market`   — the market the region's anchor tile clears at
///                  (`market_for_tile`): a measurement for the ruling's second
///                  reading.
/// A region with no reach (no tile, a lake with no ground near, no market)
/// prices at the world's price, and the budget counts how many centres did.
enum class charter_price_reach : std::uint8_t
{
    world    = 0,
    landmass = 1,
    market   = 2,
};

inline const char* charter_price_reach_name(charter_price_reach r)
{
    switch (r)
    {
    case charter_price_reach::world:    return "world";
    case charter_price_reach::landmass: return "landmass";
    case charter_price_reach::market:   return "market";
    }
    return "?";
}

/// The shipped reach (BL-1168): the landmass.
inline constexpr charter_price_reach k_stockpile_charter_reach = charter_price_reach::landmass;

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
                          ///< (`centres_razed` > 0): the points went with the towns (NR-901;
                          ///< the works themselves stand, Ben 2026-09-27)
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
    /// INDUSTRIALISATION.md § 1): the world's WHOLE stockpile (`points_total`, every
    /// region's points, the ones no centre took included) divided by
    /// `price_divisor`, in whole points, and never below 1 — fixed here, once,
    /// when the budget is built. So the seat menu is about the same size on every
    /// world: a centre's points and the price both scale with the stock. 0 on
    /// every EMPTY budget — the span off, a stock whose every point went unspent
    /// (all razed or dropped), and a rejection — because an empty budget is
    /// today's world and prices nothing. Under a reach reading (BL-1168) this
    /// is the WORLD's price — the dearest — and a centre pays `firm_price_at`.
    std::int32_t firm_price_points = 0;
    /// The divisor the price was derived by (the build's argument); 0 wherever
    /// the price is.
    std::int64_t price_divisor     = 0;

    /// BL-1168 — THE PRICE BY TRADE REACH. `reach` is the reading the prices
    /// below were taken by; under `world` both maps are empty and every centre
    /// pays `firm_price_points`. Otherwise every budgeted centre has an entry
    /// in `centre_firm_price`: its reach's stock over `price_divisor`, floored,
    /// never below 1, never above `firm_price_points`. `reach_stock` is each
    /// reach key's stock (a landmass label, or a market id), ascending key.
    /// `centres_unreached` counts budgeted centres whose region had no reach
    /// and so priced at the world's price. All empty / 0 on an empty budget.
    charter_price_reach                       reach = charter_price_reach::world;
    std::map<entity_id, std::int32_t>   centre_firm_price;
    std::map<std::int64_t, std::int64_t> reach_stock;
    std::map<entity_id, std::int64_t>   centre_reach;   ///< each budgeted centre's reach key (-1: none)
    int                                 centres_unreached = 0;

    /// The firm price at @p centre: its reach price, else the world's.
    std::int32_t firm_price_at(entity_id centre) const
    {
        const auto it = centre_firm_price.find(centre);
        return it == centre_firm_price.end() ? firm_price_points : it->second;
    }

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
//
// `k_stockpile_price_divisor` (650, NR-907/NR-910/NR-914) and the running-price
// arithmetic live in `charter_price.hpp` (included above) since BL-1099: the
// Industrialisation span prices its works-chartered notes by the same divisor
// every year, and the sim cannot include this header. The ruling and its
// readings are on the constant there; nothing about the price moved.

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
///
/// @p reach (BL-1168) names whose stock a centre's charters are priced by
/// (`charter_price_reach`); the world overload reads each region's reach key off the
/// world (`stockpile_region_reach`). A world with no home-body grid to read a
/// reach off prices every centre at the world's price (`reach` reads `world`).
stockpile_budget build_stockpile_budget(const world& w,
                                        std::int64_t price_divisor = k_stockpile_price_divisor,
                                        charter_price_reach reach = k_stockpile_charter_reach);

/// The same builder over its three inputs, for a caller holding them apart from
/// a world (tools/verify/stockpile_budget_check's hand-built slots). @p regions
/// may be null (no settlement record: an empty budget). @p region_reach, when
/// non-null and @p reach is not `world`, is each region's reach key (indexed as
/// @p regions; a key < 0 is no reach) — a vector of the wrong size REJECTS the
/// budget; null or `world` prices every centre at the world's price.
stockpile_budget build_stockpile_budget(const std::vector<region>*            regions,
                                        const std::map<entity_id, carve_slot>& founded,
                                        const std::vector<carve_dropped_slot>& dropped,
                                        std::int64_t price_divisor = k_stockpile_price_divisor,
                                        const std::vector<std::int64_t>* region_reach = nullptr,
                                        charter_price_reach reach = charter_price_reach::world);

/// BL-1168: each settlement region's reach key on @p w under @p reach — a
/// landmass label, or a market id — indexed as `gen_settlement->regions`; -1
/// where a region has none. Empty under `world`, or with no settlement record
/// or no home-body grid. Pure: reads the home body's tiles (their substrate,
/// or the market each clears at) and the regions' anchors.
std::vector<std::int64_t> stockpile_region_reach(const world& w, charter_price_reach reach);

/// The same over @p regions held apart from the world — generation's carve
/// stage, before `world::gen_settlement` is filled (hard_coded_world.cpp), so
/// the carve's plan prices exactly as the close's spend does. Empty under
/// `world` or with no home-body grid.
std::vector<std::int64_t> stockpile_region_reach(const world& w, const std::vector<region>& regions,
                                                 charter_price_reach reach);

// --- THE SPEND ---------------------------------------------------------------
//
// THE SHIPPED SPEND'S NAMED CONSTANTS, SET HERE AND NOT IN charter_budget.hpp
// (whose prices have no shipped default by design). Every one is RULED
// (INDUSTRIALISATION.md § 1): the specialist's price anchored to the seat menu and the
// square root's base (Ben, 2026-09-21, NR-910), the density ceiling (NR-902).
// The FIRM price is not a constant at all: it is derived from the stockpile
// within each centre's trade reach by `k_stockpile_price_divisor` (above, NR-907;
// the reach is BL-1168's, Ben 2026-10-03). They are read only
// when the budget is non-empty — a world the Industrialisation span ran on; with the
// span off no price is read.

/// Firm charters one specialist costs: 44 (Ben, 2026-10-03, option a; BL-1168
/// folding BL-1151). A specialist's price is this many DERIVED firm prices
/// (BL-1039's structure, NR-907), and this is the knob anchored to the SEAT MENU
/// (Ben, 2026-09-18 and 2026-09-21, NR-908): with the divisor above, it sets the
/// share of the stock a seat costs, and the seats turn on the ratio d/m alone.
///
/// PROVENANCE: NR-910 (Ben, 2026-09-21) pinned this at TWO under the world's
/// price, with the divisor taking the last step to the anchor (whole charters
/// were too coarse: at the divisor that runs the legacy tick, three opened a
/// median of about four seats and two about thirteen). That pin is OVERTURNED
/// (Ben, 2026-10-03, option a), the divisor's 650 kept.
///
/// RE-ANCHORED TO 44 (SETTLED, Ben 2026-10-03, option a: seats move off the
/// heartland). The rule is NR-908's: this knob answers the seat
/// menu, the divisor answers live-play cost. The anchor is the NO-BUDGET world
/// of the shipped arc (the span on, no budget: the world NR-910's fallback
/// lays), whose library median is 8 seats (16 seeds; the span-off legacy
/// roster's is 12). Read on the reach price (`stockpile_budget_check
/// --seat-curve`, landmass reach, d = 650), the median library world's
/// affording centres run 158.5 / 50 / 17.5 / 12 / 10 / 9 / 8 at m = 2 / 8 / 24
/// / 32 / 38 / 42 / 44 — 44 is the first whole charter at the anchor. THE
/// DIVISOR COULD NOT TAKE IT: at m = 2 the anchor needs d near 36, and there the
/// web starves — 14 to 80 background firms where d = 650 fills the 120 ceiling
/// (player_seed_sweep --charter-cost, seeds 0, 12, 28, 46). The spread stays
/// accepted (NR-910): 1 to 29 seats, the one-seat world a single landmass
/// whose capital towers (seed 11).
inline constexpr std::int32_t k_stockpile_specialist_firm_charters = 44;
/// The per-good cap's floor and the square root's base c: 8, Pass 6's legacy
/// per-good cap (Ben, 2026-09-21, NR-910), so a body at the legacy firm spend
/// keeps the legacy cap (`charter_sqrt_per_good_cap`: cap(B_ref) == c).
inline constexpr std::int32_t k_stockpile_per_resource_firm_cap = 8;
/// The anti-runaway guard per body (Pass 6's 200).
inline constexpr std::int32_t k_stockpile_max_firms_per_body = 200;
/// The density ceiling under the ruled square-root rule: 120 background firms
/// per body. RULED (Ben, 2026-09-19, NR-902; INDUSTRIALISATION.md § 1): on the cost
/// table's square-root rows the ceiling is what binds — at four times the
/// reference budget it trims every good evenly to 12 firms — while 160 never
/// bound (each good's own cap of 15 filled first) and cost a 12-31% dearer
/// economy tick for it. A ceiling that never binds is not a brake.
inline constexpr std::int32_t k_stockpile_density_ceiling = 120;

/// The spend @p sb is charged at: its own DERIVED firm price (BL-1064), the
/// ruled `sqrt_capital` cap rule under the constants above, window 4, the
/// province cap on (§ 1: the per-province cap scales with the province's centre,
/// two firms per rung — BL-1146, Ben 2026-09-27, NR-960, superseding the flat 2).
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
/// search, and once the Industrialisation span runs by default their worlds carry a
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
