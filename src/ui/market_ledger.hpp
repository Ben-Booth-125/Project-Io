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
///                potential-trade derivation prices real trade legs through
///                `price_trade_leg`, which takes a `world&` because it warms the
///                A* path cache. That call mutates no game state (supply_system.hpp
///                says so in as many words), and pricing through the same function
///                the trade pass bills with is the whole point — there is no second
///                haulage model for the surface to disagree with. Nothing else here
///                writes the world: presses enqueue `corp_command`s for
///                `app::render` to apply, and that discipline is carried by
///                convention rather than by the type.
/// @param reg     Recipe registry — the logistics cost table a leg is priced against,
///                and each good's trade capacity (units per trade point).
/// @param s       Current UI state — mutated to track the ledger's own tab
///                (`market_ledger_view`) and to consume a pending focus request
///                (`market_ledger_focus`, BL-159) that jumps the selectors to a
///                given market and opens Trades.
/// @param history Per-market, per-resource price / supply / demand time series for
///                trend plots (BL-063). Empty series render as "(no data yet)".
/// @param open    Open/closed flag; cleared by the close button.
///
/// The TRADES tab (BL-1269; docs/economy/TRADE.md) reads the player's trade
/// points and MANUAL trades from `world::trades` and, since trades are world
/// state, ADDS/REMOVES them and sets the reserve by enqueuing a `corp_command`
/// (`set_trade` / `clear_trade` / `set_trade_reserve`) onto
/// `s.pending_order_commands` — `app::render` applies it through
/// `apply_corp_command`, the same call the rival-corp scorer makes.
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

/// One MANUAL trade as drawn (BL-1269; TRADE.md § A trade) — reads 1 and 2
/// share this row shape, and the `mine` flag is what separates them. A row is a
/// `standing_trade` from `world::trades` touching the selected market (it leaves
/// it or lands on it), with its two ends named by city (`market_city_name`).
struct trade_row_record
{
    std::uint32_t trade_id = 0;                ///< `standing_trade::id` — what clear_trade names.
    entity_id     corp     = null_entity;      ///< The owner.
    std::string   corp_name;                   ///< Owner's display name; "Corp #n" fallback.
    resource_type resource = resource_type::iron_ore;
    const char*   name     = "";
    entity_id     from_market = null_entity;
    std::string   from_name;                   ///< `market_city_name` of the source.
    entity_id     to_market   = null_entity;
    std::string   to_name;                     ///< `market_city_name` of the destination.
    float         points   = 0.0f;             ///< Trade points assigned per tick.
    float         units    = 0.0f;             ///< points x capacity(resource): the most it ships a tick.
    bool          mine     = false;            ///< Read 1 (the player's) vs read 2 (the market's).
};

/// The player's TRADE POINTS as drawn (TRADE.md § Auto and reserved trade): what
/// its Marketplaces and Ports made on the last trade pass, the reserve it holds
/// back for manual trades (as set, and as clamped to what it makes), what the
/// manual trades ask for, and the rest — auto.
struct trade_points_record
{
    bool  drawn         = false; ///< False when the Trades tab was not on screen.
    float made          = 0.0f;  ///< `corporation_component::trade_points`.
    float reserve       = 0.0f;  ///< `corporation_component::trade_reserve`, as set.
    float reserve_used  = 0.0f;  ///< min(reserve, made) — what the manual trades can spend.
    float auto_points   = 0.0f;  ///< made - reserve_used: the rest is auto.
    float manual_asked  = 0.0f;  ///< Sum of the player's manual trades' points, everywhere.
    int   manual_count  = 0;     ///< The player's manual trades, everywhere.
};

/// The trade-points read as last drawn.
const trade_points_record& trade_points_read();

/// One row of the potential-trades DERIVATION — read 3, and the only one with no
/// store behind it.
///
/// `margin` is `sell_price - buy_price - haulage`, per unit, and every term is a
/// real read: the two prices come from the two `market_component`s and `haulage`
/// is `price_trade_leg`'s own cost for a one-unit leg, so the figure a player
/// acts on is the figure the trade pass would charge them. A leg that will not
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

/// Read 1 — the player's manual trades touching the selected market, as drawn.
const std::vector<trade_row_record>& my_trades();

/// Read 2 — every manual trade touching that market, whoever owns it. EMPTY when
/// the gate is shut; `market_trades_open()` distinguishes "shut" from "none standing".
const std::vector<trade_row_record>& market_trades();

/// Whether read 2's gate is open: THE PLAYER OWNS A BUILDING ON THAT BODY (Ben,
/// 2026-08-29, choosing it over "an order here", "either", and "any discovered
/// market"). A real predicate, enforced rather than assumed — a player reads the
/// trades of markets they operate in, not of the whole system.
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
