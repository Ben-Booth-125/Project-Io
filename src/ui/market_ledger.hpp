#pragma once

#include "plot_history.hpp"
#include "ui_state.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <string>
#include <vector>

namespace ui {

/// Draws the Market Ledger window. open controls visibility (toggled by nav rail).
///
/// @param w       World. NON-CONST, and for exactly one reason: the Trades tab's
///                potential-trade derivation prices real convoy legs through
///                `price_convoy_leg`, which takes a `world&` because it warms the
///                A* path cache. That call mutates no game state (supply_system.hpp
///                says so in as many words), and pricing through the same function
///                the auto-dispatcher and the player's `dispatch_convoy` verb use
///                is the whole point — there is no second haulage model for the
///                surface to disagree with. Nothing else here writes the world:
///                presses still enqueue `corp_command`s for `app::render` to apply,
///                and that discipline is now carried by convention rather than by
///                the type.
/// @param reg     Recipe registry — the logistics cost table a leg is priced against.
/// @param s       Current UI state — mutated to track the ledger's own tab
///                (`market_ledger_view`) and to consume a pending focus request
///                (`market_ledger_focus`, BL-159) that jumps the selectors to a
///                given market and opens Trades.
/// @param history Per-market, per-resource price / supply / demand time series for
///                trend plots (BL-063). Empty series render as "(no data yet)".
/// @param open    Open/closed flag; cleared by the close button.
///
/// The TRADES tab reads standing orders from `world::sell_orders` AND
/// `world::buy_orders` (BL-293) and, since the book is world state, ADDS/REMOVES by
/// enqueuing a `corp_command` onto `s.pending_order_commands` — `app::render`
/// applies the request through `apply_corp_command`, the same call the rival-corp
/// scorer makes.
void draw_market_ledger(world& w,
                        const recipe_registry& reg,
                        ui_state& s,
                        const market_plot_history& history,
                        bool& open);

/// One row of the Goods table AS DRAWN (BL-686). Recorded by `draw_goods_tab`
/// each frame and read by the verify layer.
///
/// It records what the surface actually computed and put on screen, so a check
/// can cross it against `market_component` rather than re-deriving the same
/// figures and comparing them to themselves — which is what a check written
/// against the world alone would do, and it would pass against a table that drew
/// nothing at all. `expect_no_clipping` is vacuous on this class of surface
/// (NR-663: zero records over visibly clipped frames), so this is the assertion
/// that has to carry the weight.
struct goods_row_record
{
    resource_type resource{};
    const char*   name        = "";
    float         price       = 0.0f;  ///< `market_component::price[r]` as drawn.
    float         base_price  = 0.0f;  ///< `market_component::base_price[r]`.
    float         body_avg    = 0.0f;  ///< Mean price across the body's markets.
    float         vs_base     = 0.0f;  ///< `price / base_price`, the drawn column.
    int           samples     = 0;     ///< Points the row's graph plotted (<= 8).
    float         name_avail  = 0.0f;  ///< Px the name column actually offered.
    float         name_needed = 0.0f;  ///< Px the full name needed at this font.
};

/// One chip of the nation presence row (BL-688), as drawn.
struct nation_chip_record
{
    entity_id   nation = null_entity;
    std::string name;
    std::string initials;
};

/// The nation presence row's chips as last drawn, in draw order.
const std::vector<nation_chip_record>& nation_chips();

/// The Goods table's rows as last drawn, in draw order. Empty when the Goods view
/// was not on screen.
const std::vector<goods_row_record>& goods_rows();

/// The market the ledger last drew the Goods table for (`null_entity` if none).
entity_id goods_market();

// ---------------------------------------------------------------------------
// The Trades tab (BL-687) — its three reads and the history half, AS DRAWN
// ---------------------------------------------------------------------------
// `MARKETS.md` § Trades owns the shape. The three reads are NOT equally cheap and
// the surface must not present them as one undifferentiated table, so they are
// three record types rather than one with a discriminator — a check that cannot
// tell them apart could not assert the thing the design is actually about.
//
// Same drawn-vs-world discipline as `goods_row_record`: these record what the
// surface computed and put on screen, so an assertion crosses them against world
// state rather than restating it (`expect_no_clipping` is vacuous on this class,
// NR-663).

/// One standing order as drawn — reads 1 and 2 share this row shape, and the
/// `mine` flag is what separates them.
///
/// DIRECTION IS CARRIED BY THE LIMIT, not by a column of its own: a sell reads
/// ">= floor" and a buy "<= ceiling", exactly as the pre-rename Sell Orders row
/// always did. At ~380 px of column a fourth text column costs more than it says.
struct trade_row_record
{
    std::uint32_t order_id = 0;                ///< `sell_order::id` / `buy_order::id`.
    entity_id     corp     = null_entity;      ///< The owner. An ORDER always has one.
    std::string   corp_name;                   ///< Owner's display name; "Corp #n" fallback.
    resource_type resource = resource_type::iron_ore;
    const char*   name     = "";
    bool          is_buy   = false;            ///< False = sell (floor), true = buy (ceiling).
    float         quantity = 0.0f;
    float         limit_price = 0.0f;          ///< `floor_price` on a sell, `max_price` on a buy.
    bool          mine     = false;            ///< Read 1 (the player's) vs read 2 (the market's).
};

/// One row of the potential-trades DERIVATION — read 3, and the only one with no
/// store behind it.
///
/// `margin` is `sell_price - buy_price - haulage`, per unit, and every term is a
/// real read: the two prices come from the two `market_component`s and `haulage`
/// is `price_convoy_leg`'s own cost for a one-unit leg, so the figure a player
/// acts on is the figure the dispatcher would charge them. A leg that will not
/// price (no anchor, no route, no pad, no propellant) produces NO ROW — an
/// unreachable market is not a trade at a worse margin, it is not a trade.
struct potential_trade_record
{
    resource_type resource     = resource_type::iron_ore;
    const char*   name         = "";
    entity_id     dest_market  = null_entity;
    std::string   dest_name;                   ///< `market_city_name` of the destination.
    float         buy_price    = 0.0f;         ///< `price[r]` at the SELECTED market.
    float         sell_price   = 0.0f;         ///< `price[r]` at the destination.
    float         haulage      = 0.0f;         ///< Per-unit haul cost on the priced leg.
    float         margin       = 0.0f;         ///< sell - buy - haulage, per unit.
    int           travel_ticks = 1;            ///< Quarters the leg takes.
};

/// One row of the exchange-record read — the history half, over `world::exchanges`.
///
/// THE COLUMN IS REVENUE, NEVER PROFIT, and that limit is structural rather than
/// an omission: `stockpile_component` is `quantities[]` and nothing else, so there
/// is no cost basis anywhere in the model and the margin on a sale cannot be
/// derived from the sale. `quantity * unit_price` is honest. There is deliberately
/// no `profit` field here, because a field would eventually get printed.
///
/// A `null_entity` counterparty MEANS THE MARKET and not "unknown" — three of the
/// four clearing paths trade against the market as counterparty of last resort and
/// they carry the volume, so a reader that blanked those rows would empty the tab.
/// The `*_is_market` flags say which side that was; the name strings already read
/// "Market".
struct exchange_row_record
{
    int           tick       = 0;              ///< Econ tick == quarter. Rendered as a qtr.
    resource_type resource   = resource_type::iron_ore;
    const char*   name       = "";
    float         quantity   = 0.0f;
    float         unit_price = 0.0f;
    float         revenue    = 0.0f;           ///< quantity * unit_price. NOT a margin.
    std::string   seller;                      ///< "Market" when the side is null_entity.
    std::string   buyer;
    bool          seller_is_market = false;
    bool          buyer_is_market  = false;
};

/// Read 1 — the player's standing trades at the selected market's body, as drawn.
const std::vector<trade_row_record>& my_trades();

/// Read 2 — every standing trade on that body, whoever owns it. EMPTY when the
/// gate is shut; `market_trades_open()` distinguishes "shut" from "none standing".
const std::vector<trade_row_record>& market_trades();

/// Whether read 2's gate is open: THE PLAYER OWNS A BUILDING ON THAT BODY (Ben,
/// 2026-08-29, choosing it over "an order here", "either", and "any discovered
/// market"). A real predicate, enforced rather than assumed — a player reads the
/// books of markets they trade at, not of the whole system.
bool market_trades_open();

/// Read 3 — the potential-trade derivation, ordered by margin, best first.
/// Ranking is permitted HERE and only here (`CONCEPT.md` § Player identity: rank
/// where the top row is one input among several, not where it IS the move).
const std::vector<potential_trade_record>& potential_trades();

/// The history half — exchanges at the selected market, NEWEST FIRST.
const std::vector<exchange_row_record>& exchange_rows();

/// The market the Trades tab last drew for (`null_entity` if it was not on screen).
entity_id trades_market();

/// The city name of a market — the population centre anchoring its `centre_tile`
/// (`world::population_centre_name`), or the body name as a fallback when the market is
/// unanchored / unnamed. This is the market/city identity shown in the ledger's second
/// selector and emitted by the CSV export. See generation city naming.
std::string market_city_name(const world& w, entity_id market_id);

} // namespace ui
