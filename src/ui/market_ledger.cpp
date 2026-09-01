#include "market_ledger.hpp"

#include "foldout_column.hpp" // shell fold-out column host (BL-122)
#include "format.hpp"         // fmt::abbreviate — the Revenue column's width budget
#include "icons.hpp"
#include "plot_history.hpp"
#include "presentation.hpp"
#include "text_fit.hpp"

#include "world/market_clearing.hpp" // market_for_tile — the nation presence row
#include "world/supply_system.hpp"  // price_convoy_leg — the Trades tab's haulage term

#include <imgui.h>

#include <algorithm>
#include <cctype>  // std::toupper — the nation chip's initials
#include <cmath>
#include <cstdio>  // std::snprintf
#include <cstdint>
#include <string>
#include <vector>

namespace ui {

namespace {

/// Body display name, or a synthetic fallback.
std::string body_label(const world& w, entity_id body)
{
    const auto it = w.bodies.find(body);
    if (it != w.bodies.end())
        return it->second.name;
    return "Body #" + std::to_string(body);
}

/// Markets on a given body, sorted by ascending entity id (deterministic).
std::vector<entity_id> markets_on_body(const world& w, entity_id body)
{
    std::vector<entity_id> result;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body)
            result.push_back(mid);
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace

std::string market_city_name(const world& w, entity_id mid)
{
    const auto mit = w.markets.find(mid);
    if (mit == w.markets.end())
        return "Market #" + std::to_string(mid);
    const market_component& mc = mit->second;
    // Resolve the population centre anchoring the market's centre tile → its city name.
    if (mc.centre_tile != null_entity)
        for (const auto& [cid, tid] : w.population_centre_tile)
            if (tid == mc.centre_tile)
            {
                const auto nit = w.population_centre_name.find(cid);
                if (nit != w.population_centre_name.end() && !nit->second.empty())
                    return nit->second;
                break;
            }
    return body_label(w, mc.body); // unanchored / unnamed fallback
}

namespace {

// --- The Trades tab (BL-687) --------------------------------------------------
// "What positions do I hold, what else is standing here, and what could I be
// doing?" — plus what actually moved. `MARKETS.md` § Trades and § The exchange
// record own the design; this is that design as four sections.
//
// IT IS CALLED TRADES, NOT SELL ORDERS (Ben, 2026-08-29, explicitly). The word
// carries the widening: a sell order is one direction and one actor, a trade is a
// position either way round held by anyone in the market. The buy side is
// admitted here as a READ — `world::buy_orders` is real world state that the
// clearing algorithm honours — even though no press and no corp_verb writes one
// yet, so the section is normally the sell book alone. A reader that only walked
// `sell_orders` would silently under-report the moment BL-160's auto-exchange
// policy starts emitting bids.
//
// THE THREE READS ARE NOT EQUALLY CHEAP AND ARE NOT ONE TABLE. Read 1 is a filter
// on the player's own orders; read 2 is the same book past a gate; read 3 is a
// derivation with no store behind it at all. They get three headed sections and
// three record types, because presenting them as one list would be claiming they
// cost the same to know.
//
// Relocated here from the Construction/Building panel by BL-159 and moved onto
// world state by BL-293: the press composes a `corp_command` and `app::render`
// applies it through `apply_corp_command`, the same call a rival's scorer makes.
// A player and an AI cannot diverge, because there is nothing to diverge.

/// How many exchange rows the history section keeps. The ring holds up to 8192
/// world-wide; a column ~380 px wide is not a place to scroll thousands of rows,
/// and the newest are the ones a trading decision is taken against.
constexpr std::size_t k_history_rows = 120;

/// How many potential trades survive the ranking. Ranked, so the tail is by
/// construction the part nobody reads.
constexpr std::size_t k_potential_rows = 40;

std::vector<trade_row_record>       g_my_trades;
std::vector<trade_row_record>       g_market_trades;
std::vector<potential_trade_record> g_potential;
std::vector<exchange_row_record>    g_exchanges;
bool                                g_market_trades_open = false;
entity_id                           g_trades_market      = null_entity;

// The potential/history cache key (see `draw_trades_tab`). Namespace-scope rather
// than function-local so leaving the tab can INVALIDATE it: the leave path clears
// the record vectors so a stale frame cannot be read as this one's, and a key that
// still matched would then leave those vectors empty on return.
entity_id   g_cache_market  = null_entity;
int         g_cache_tick    = -1;
std::size_t g_cache_markets = 0;
std::size_t g_cache_assets  = 0;

/// Drop every Trades record and the cache key with them. Called when the tab is
/// not the one on screen.
void clear_trade_records()
{
    g_my_trades.clear();
    g_market_trades.clear();
    g_potential.clear();
    g_exchanges.clear();
    g_market_trades_open = false;
    g_trades_market      = null_entity;
    g_cache_market       = null_entity;
    g_cache_tick         = -1;
    g_cache_markets      = 0;
    g_cache_assets       = 0;
}

/// A corporation's display name; "Corp #n" when it has none.
std::string corp_display_name(const world& w, entity_id id)
{
    const auto it = w.corporations.find(id);
    if (it != w.corporations.end() && !it->second.name.empty())
        return it->second.name;
    return "Corp #" + std::to_string(id);
}

/// The name a corporation is drawn under IN A CELL — its first word.
///
/// MEASURED, not guessed. At the shipped column width the counterparty cell gets
/// roughly 110 px, and the full generated names ("Hexel Systems", "Bryur
/// Dynamics", "Corix Industries") elide to "Hexel Sys...", which is exactly the
/// defect the Goods table's name column was rebuilt to avoid: two firms whose
/// names share a prefix draw as one string. The distinguishing word is the first
/// one — the second is "Systems" / "Industries" / "Dynamics" over and over — so
/// the cell carries the head and the record and the hover carry the whole thing.
/// A single-word name is left alone and elides as before.
std::string short_corp_name(const std::string& full)
{
    const std::size_t sp = full.find(' ');
    return (sp == std::string::npos) ? full : full.substr(0, sp);
}

/// Height for a table of @p rows plus its header row — the budget a bounded
/// section gets. In LINE HEIGHTS, so it follows the font rather than a pixel
/// count authored against one.
float table_height(int rows)
{
    return ImGui::GetTextLineHeightWithSpacing() * static_cast<float>(rows + 1) + 6.0f;
}

/// One side of an exchange, as a label.
///
/// `null_entity` MEANS THE MARKET, not "unknown" (`MARKETS.md` § The exchange
/// record). Only the matched order-book path has a real corp on both sides and it
/// is dormant in play; the three paths that carry the volume — a corp's
/// auto-surplus sold TO the market, a processor's input drawn FROM it, an
/// unmatched standing sell auto-cleared to it — leave one side empty. Rendering
/// that as "unknown" would be wrong, and skipping those rows would leave the
/// section nearly empty.
std::string counterparty_label(const world& w, entity_id id, bool& is_market)
{
    is_market = (id == null_entity);
    if (is_market)
        return "Market";
    return corp_display_name(w, id);
}

/// READ 2's GATE: does the player own a building on this body?
///
/// Ben's choice, 2026-08-29, over "an order here", "either", and "any discovered
/// market". Orders are world state and the deliberate public signal, so this is a
/// reading question rather than a disclosure one — but *operates in* is a real
/// predicate and it is ENFORCED here rather than assumed: a player reads the books
/// of markets they trade at, not of the whole system.
///
/// Grounded on the two things that exist — `corporation_component::assets` is the
/// live building list (construction pushes and erases it) and a building's tile
/// carries its body. No second notion of "operating".
bool player_operates_on_body(const world& w, entity_id body)
{
    const auto cit = w.corporations.find(w.player_entity);
    if (cit == w.corporations.end())
        return false;
    for (const entity_id bid : cit->second.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const auto tit = w.tiles.find(bit->second.tile);
        if (tit != w.tiles.end() && tit->second.body == body)
            return true;
    }
    return false;
}

/// Both books on one body, as rows. `mine_only` filters to the player's corp.
std::vector<trade_row_record> collect_trades(const world& w, entity_id body, bool mine_only)
{
    const entity_id player = w.player_entity;
    std::vector<trade_row_record> out;

    // Sells first, then buys, each in book order — insertion order is SEMANTIC
    // (price-time priority, `MARKETS.md` § Where the order book lives), so the
    // list is never re-sorted and what the player reads is the queue that clears.
    for (const sell_order& o : w.sell_orders)
    {
        if (o.body != body || (mine_only && o.corp != player))
            continue;
        trade_row_record r;
        r.order_id    = o.id;
        r.corp        = o.corp;
        r.corp_name   = corp_display_name(w, o.corp);
        r.resource    = o.resource;
        r.name        = presentation_of(o.resource).name;
        r.is_buy      = false;
        r.quantity    = o.quantity;
        r.limit_price = o.floor_price;
        r.mine        = (o.corp == player);
        out.push_back(r);
    }
    for (const buy_order& o : w.buy_orders)
    {
        if (o.body != body || (mine_only && o.corp != player))
            continue;
        trade_row_record r;
        r.order_id    = o.id;
        r.corp        = o.corp;
        r.corp_name   = corp_display_name(w, o.corp);
        r.resource    = o.resource;
        r.name        = presentation_of(o.resource).name;
        r.is_buy      = true;
        r.quantity    = o.quantity;
        r.limit_price = o.max_price;
        r.mine        = (o.corp == player);
        out.push_back(r);
    }
    return out;
}

/// READ 3 — the potential-trade derivation. Buy price here against sell price
/// there, LESS THE HAULAGE THE ROUTE WOULD COST.
///
/// The haulage term is `price_convoy_leg`'s own answer for a ONE-UNIT leg, so it
/// is the number the auto-dispatcher and the player's `dispatch_convoy` verb would
/// charge — not a second cost model that could disagree with the one that bills.
/// That is also why the ledger holds a non-const `world&`: the call warms the A*
/// cache and mutates no game state.
///
/// A leg that will not price produces NO ROW. An unreachable market is not a trade
/// at a worse margin; it is not a trade, and listing it with an invented haulage
/// would be exactly the invented figure this surface is under instruction to avoid.
///
/// Cost control: the gross-spread test comes BEFORE the pricing call, so the A*
/// runs at most once per destination market rather than once per (market, good).
/// The result is cached by the caller against the econ tick.
std::vector<potential_trade_record> derive_potential_trades(
    world& w, const recipe_registry& reg, entity_id src_body, entity_id here_mid)
{
    std::vector<potential_trade_record> out;

    const entity_id corp = w.player_entity;
    if (w.corporations.find(corp) == w.corporations.end())
        return out;
    const auto hit = w.markets.find(here_mid);
    if (hit == w.markets.end())
        return out;
    const market_component& here = hit->second;

    const logistics_nodes nodes      = collect_logistics_nodes(w);
    const float           space_cost = reg.logistics_cost(convoy_mode::space);

    // Ascending market id — deterministic, and the same order on every frame.
    std::vector<entity_id> dests;
    for (const auto& [mid, mc] : w.markets)
    {
        (void)mc;
        if (mid != here_mid)
            dests.push_back(mid);
    }
    std::sort(dests.begin(), dests.end());

    for (const entity_id dm : dests)
    {
        const market_component& there = w.markets.at(dm);
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (here.base_price[r] <= 0.0f || there.base_price[r] <= 0.0f)
                continue; // not traded at both ends
            const float buy  = here.price[r];
            const float sell = there.price[r];
            if (!(sell > buy))
                continue; // no gross spread — prune before paying for a path

            const convoy_leg leg = price_convoy_leg(w, reg, nodes, corp, src_body, dm,
                                                    r, 1.0f, space_cost);
            if (!leg.viable)
                continue;

            const float margin = sell - buy - leg.cost;
            if (!(margin > 0.0f))
                continue; // the haulage ate the spread

            potential_trade_record rec;
            rec.resource     = static_cast<resource_type>(r);
            rec.name         = presentation_of(rec.resource).name;
            rec.dest_market  = dm;
            rec.dest_name    = market_city_name(w, dm);
            rec.buy_price    = buy;
            rec.sell_price   = sell;
            rec.haulage      = leg.cost;
            rec.margin       = margin;
            rec.travel_ticks = leg.travel_ticks;
            out.push_back(rec);
        }
    }

    // RANKING IS PERMITTED HERE, and this is the one surface where that has been
    // ruled on explicitly (`CONCEPT.md` § Player identity, and Ben the same day:
    // "Market prices is a vital pillar of gameplay, but the strategy 'just build
    // the most profitable' is a red herring"). A potential trade sorted by margin
    // is one input among several — the player still weighs reach, stock,
    // competition and what the price does next — so ordering it does not decide
    // the game. Ordering TILES TO BUILD ON by margin does, and is refused.
    std::sort(out.begin(), out.end(),
              [](const potential_trade_record& a, const potential_trade_record& b) {
                  if (a.margin != b.margin)
                      return a.margin > b.margin;
                  if (a.dest_market != b.dest_market)
                      return a.dest_market < b.dest_market;
                  return a.resource < b.resource;
              });
    if (out.size() > k_potential_rows)
        out.resize(k_potential_rows);
    return out;
}

/// The history half — `world::exchanges` filtered to one market, NEWEST FIRST.
///
/// Walked through `oldest_first`, never through `entries` directly: the raw ring's
/// vector order stops being chronological the moment it wraps at 8192 rows, and a
/// reader walking the vector would show history shuffled at the wrap point.
std::vector<exchange_row_record> derive_exchange_rows(const world& w, entity_id mid)
{
    std::vector<exchange_row_record> out;
    const std::size_t n = w.exchanges.size();
    for (std::size_t k = 0; k < n && out.size() < k_history_rows; ++k)
    {
        const exchange_record& e = w.exchanges.oldest_first(n - 1 - k); // newest first
        if (e.market != mid)
            continue;
        exchange_row_record r;
        r.tick       = e.tick;
        r.resource   = e.resource;
        r.name       = presentation_of(e.resource).name;
        r.quantity   = e.quantity;
        r.unit_price = e.unit_price;
        // REVENUE. There is no cost basis anywhere in the model, so this is the
        // only honest figure a sale yields; a "profit" column would be a number
        // the clearing loop never computed and a player would act on.
        r.revenue    = e.quantity * e.unit_price;
        r.seller     = counterparty_label(w, e.seller, r.seller_is_market);
        r.buyer      = counterparty_label(w, e.buyer,  r.buyer_is_market);
        out.push_back(r);
    }
    return out;
}

/// The add-order form. Unchanged in behaviour from the pre-rename tab — it is
/// still `place_sell_order`, because that is still the only order verb a press
/// can issue. Folded behind a tree node so it does not take a fifth of the column
/// from the reads.
void draw_place_order_form(ui_state& state, entity_id corp, entity_id body,
                           const market_component& market)
{
    static int   add_resource = -1;
    static float add_quantity = 10.0f;
    static float add_floor    = 0.0f;

    // Reset the form when the selected market's body changes — the statics
    // otherwise carry one market's resource/quantity onto another.
    static entity_id form_body = null_entity;
    if (form_body != body)
    {
        form_body    = body;
        add_resource = -1;
        add_quantity = 10.0f;
        add_floor    = 0.0f;
    }

    if (add_resource < 0 || market.base_price[static_cast<std::size_t>(add_resource)] <= 0.0f)
    {
        for (std::size_t r = 0; r < resource_count; ++r)
            if (market.base_price[r] > 0.0f) { add_resource = static_cast<int>(r); break; }
    }

    const char* preview = (add_resource >= 0)
        ? resource_name(static_cast<resource_type>(add_resource)) : "-";
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
    if (ImGui::BeginCombo("Good", preview))
    {
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (market.base_price[r] <= 0.0f)
                continue;
            const bool sel = (add_resource == static_cast<int>(r));
            if (ImGui::Selectable(resource_name(static_cast<resource_type>(r)), sel))
                add_resource = static_cast<int>(r);
        }
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
    ImGui::InputFloat("Qty / qtr", &add_quantity, 1.0f, 10.0f, "%.0f");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
    ImGui::InputFloat("Floor",     &add_floor,    0.1f, 1.0f,  "%.1f");
    if (add_quantity < 0.0f) add_quantity = 0.0f;
    if (add_floor    < 0.0f) add_floor    = 0.0f;

    ImGui::BeginDisabled(add_resource < 0 || add_quantity <= 0.0f);
    if (ImGui::Button("Place sell trade"))
    {
        corp_command cmd;
        cmd.corp        = corp;
        cmd.verb        = corp_verb::place_sell_order;
        cmd.subject     = body; // place_sell_order's subject is the body
        cmd.target      = static_cast<resource_type>(add_resource);
        cmd.quantity    = add_quantity;
        cmd.floor_price = add_floor;
        state.pending_order_commands.push_back(cmd);
    }
    ImGui::EndDisabled();
}

/// One standing-order table — the row shape reads 1 and 2 share.
///
/// @param show_owner Read 2 names the owner; read 1 is the player's own book and
///                   would print the same name on every row.
/// @param removable  Read 1 only. You cannot withdraw a rival's order, and a
///                   press that could not succeed has no business being drawn.
void draw_trade_table(const char* table_id, const std::vector<trade_row_record>& rows,
                      bool show_owner, bool removable, ui_state& state, entity_id corp)
{
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
    const int n_cols = 3 + (show_owner ? 1 : 0) + (removable ? 1 : 0);
    if (!ImGui::BeginTable(table_id, n_cols, flags))
        return;

    // FIXED SIBLINGS SIZED AGAINST THE LIVE FONT AND TO THE WIDEST STRING EACH CAN
    // HOLD; the names take what is left. Budget against `shell_column_width`
    // (~380 px at 1280, 384 px at 1920 — the difference between those resolutions
    // is all VERTICAL), never against a pixel count authored at a guessed font
    // size. NR-709 is the failure this avoids, on four surfaces so far.
    //
    // NO ITEM GLYPH ON THIS TABLE, unlike the Goods board. Measured: with the
    // glyph reserved, five columns left the Good and Holder names ~77 px and
    // ~64 px, which draws "Petrol..." and "Far..." — two goods or two firms
    // sharing a prefix become one string, which is the exact defect the Goods
    // table's name column was rebuilt to fix. The glyph is a deliberate
    // PLACEHOLDER (ICONS.md § 2b), and reserving width for artwork that does not
    // exist yet at the cost of the names that do is the wrong trade on a column
    // this narrow. The good keeps its identity COLOUR on its name, which is the
    // half of the mark that carries meaning today.
    const float pad   = ImGui::GetStyle().CellPadding.x * 2.0f;
    const float w_qty = ImGui::CalcTextSize("0000").x + pad;
    const float w_lim = ImGui::CalcTextSize(">=00.0").x + pad;
    const float w_rm  = ImGui::CalcTextSize("x").x + ImGui::GetStyle().FramePadding.x * 2.0f + pad;

    // The good outweighs the holder: a good's name is the thing being compared
    // across rows, and the holder's first word identifies it in less width.
    ImGui::TableSetupColumn("Good", ImGuiTableColumnFlags_WidthStretch, 1.2f);
    if (show_owner)
        ImGui::TableSetupColumn("Holder", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("Qty",   ImGuiTableColumnFlags_WidthFixed, w_qty);
    ImGui::TableSetupColumn("Limit", ImGuiTableColumnFlags_WidthFixed, w_lim);
    if (removable)
        ImGui::TableSetupColumn("##x", ImGuiTableColumnFlags_WidthFixed, w_rm);
    ImGui::TableHeadersRow();

    for (const trade_row_record& r : rows)
    {
        // PushID on the ORDER ID, not the loop index: the id is what the remove
        // command names, and it is stable across the erase a press causes.
        ImGui::PushID(static_cast<int>(r.order_id));
        ImGui::TableNextRow();

        const resource_presentation& rp = presentation_of(r.resource);
        int col = 0;
        ImGui::TableSetColumnIndex(col++);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(rp.colour));
        ui::fit_text(ui::text_box::table_cell, "market.trades.good", r.name,
                     ImGui::GetContentRegionAvail().x);
        ImGui::PopStyleColor();

        if (show_owner)
        {
            ImGui::TableSetColumnIndex(col++);
            // The player's own row is tinted so read 2 does not bury it: "what
            // else is standing here" is read against what I hold.
            if (r.mine)
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 200, 255, 255));
            const std::string shown = short_corp_name(r.corp_name);
            ui::fit_text(ui::text_box::table_cell, "market.trades.holder", shown.c_str(),
                         ImGui::GetContentRegionAvail().x);
            if (r.mine)
                ImGui::PopStyleColor();
            // The full name is one hover away — the cell carries a handle, not
            // the identity.
            if (ImGui::BeginItemTooltip())
            {
                ImGui::TextUnformatted(r.corp_name.c_str());
                ImGui::EndTooltip();
            }
        }

        ImGui::TableSetColumnIndex(col++);
        ImGui::Text("%.0f", static_cast<double>(r.quantity));

        // DIRECTION IS THE LIMIT'S OPERATOR. ">=" is a sell's floor, "<=" a buy's
        // ceiling — the same shorthand the pre-rename row used, and it buys the
        // name column a whole text column's width.
        ImGui::TableSetColumnIndex(col++);
        ImGui::TextDisabled(r.is_buy ? "<=%.1f" : ">=%.1f", static_cast<double>(r.limit_price));

        if (removable)
        {
            ImGui::TableSetColumnIndex(col);
            if (r.is_buy)
            {
                // No verb removes a buy order — the buy side has no emitter and
                // no remover (`MARKETS.md` § Where the order book lives). A press
                // that could not succeed is not drawn.
                ImGui::TextDisabled("-");
            }
            else if (ImGui::SmallButton("x"))
            {
                corp_command cmd;
                cmd.corp  = corp;
                cmd.verb  = corp_verb::remove_sell_order;
                cmd.order = r.order_id;
                state.pending_order_commands.push_back(cmd);
            }
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

/// READ 3's table. Ranked by margin, best first, with the three terms behind a
/// hover rather than in three more columns the width cannot hold.
void draw_potential_table(const std::vector<potential_trade_record>& rows)
{
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable("##potential", 3, flags))
        return;

    const float pad    = ImGui::GetStyle().CellPadding.x * 2.0f;
    const float w_marg = ImGui::CalcTextSize("+000.00").x + pad;

    // No glyph column here either, and for the same measured reason as the
    // standing-trade table: two names share this row and the placeholder mark is
    // not worth a fifth of one of them.
    ImGui::TableSetupColumn("Good",    ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("To",      ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("Margin",  ImGuiTableColumnFlags_WidthFixed, w_marg);
    ImGui::TableHeadersRow();

    int id = 0;
    for (const potential_trade_record& r : rows)
    {
        ImGui::PushID(id++);
        ImGui::TableNextRow();

        const resource_presentation& rp = presentation_of(r.resource);
        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(rp.colour));
        ui::fit_text(ui::text_box::table_cell, "market.trades.potential_good", r.name,
                     ImGui::GetContentRegionAvail().x);
        ImGui::PopStyleColor();

        ImGui::TableSetColumnIndex(1);
        ui::fit_text(ui::text_box::table_cell, "market.trades.potential_to", r.dest_name.c_str(),
                     ImGui::GetContentRegionAvail().x);

        ImGui::TableSetColumnIndex(2);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(IM_COL32(90, 200, 140, 255)),
                           "+%.2f", static_cast<double>(r.margin));

        // The three terms, one hover away — the row is a ranking, the tooltip is
        // the arithmetic behind it, so nothing is asserted that cannot be checked.
        if (ImGui::BeginItemTooltip())
        {
            ImGui::Text("%s to %s", r.name, r.dest_name.c_str());
            ImGui::Separator();
            ImGui::Text("Buy here      %.2f", static_cast<double>(r.buy_price));
            ImGui::Text("Sell there    %.2f", static_cast<double>(r.sell_price));
            ImGui::Text("Haulage /unit %.2f", static_cast<double>(r.haulage));
            ImGui::Text("Margin /unit  %.2f", static_cast<double>(r.margin));
            ImGui::Text("Arrives in    %d qtr", r.travel_ticks);
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

/// The history half's table.
///
/// THE COLUMN IS HEADED "Revenue" AND THAT IS NOT A SHORTENING OF SOMETHING ELSE.
/// `stockpile_component` is `quantities[]` and nothing else, so nothing in the
/// model knows what a unit cost to acquire and no margin is derivable from a sale.
/// `quantity * unit_price` is the honest figure and it is what this prints.
void draw_history_table(const std::vector<exchange_row_record>& rows)
{
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable("##exchanges", 4, flags))
        return;

    const float pad   = ImGui::GetStyle().CellPadding.x * 2.0f;
    const float w_qtr = ImGui::CalcTextSize("q000").x + pad;
    // ABBREVIATED, so the column is 5 characters wide instead of however many
    // digits a quarter's turnover happens to have. `fmt::abbreviate` is the
    // shell's own compact form ("1.2k", "3.4M"), and the exact figure is in the
    // row's hover — which is where a number a player wants to the credit belongs
    // on a 380 px column anyway.
    // Sized to the HEADER where the header is the longer string. "Revenue" is a
    // word this column is not allowed to shorten — it is the honest name for what
    // the figure is — so the column takes the width the word needs.
    const float w_rev = std::max(ImGui::CalcTextSize("000.0k").x,
                                 ImGui::CalcTextSize("Revenue").x) + pad;

    // "qtr", never "tick" — an econ tick IS a calendar quarter and the display
    // word is the calendar one.
    ImGui::TableSetupColumn("qtr",     ImGuiTableColumnFlags_WidthFixed, w_qtr);
    ImGui::TableSetupColumn("Good",    ImGuiTableColumnFlags_WidthStretch, 0.9f);
    // The widest stretch, because it carries the counterparty. Even so it only
    // fits by dropping the redundant half of the pair; the full pair, labelled
    // seller and buyer, is in the hover.
    ImGui::TableSetupColumn("With",    ImGuiTableColumnFlags_WidthStretch, 1.6f);
    ImGui::TableSetupColumn("Revenue", ImGuiTableColumnFlags_WidthFixed, w_rev);
    ImGui::TableHeadersRow();

    int id = 0;
    for (const exchange_row_record& r : rows)
    {
        ImGui::PushID(id++);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("q%d", r.tick);

        const resource_presentation& rp = presentation_of(r.resource);
        ImGui::TableSetColumnIndex(1);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(rp.colour));
        ui::fit_text(ui::text_box::table_cell, "market.trades.hist_good", r.name,
                     ImGui::GetContentRegionAvail().x);
        ImGui::PopStyleColor();

        // BOTH SIDES, AND AN ABSENT SIDE READS "Market". It is the market itself,
        // not an unknown party: three of the four clearing paths trade against the
        // market as counterparty of last resort and they carry the volume, so a
        // row that read "unknown" would be wrong and one that was skipped would
        // empty the section.
        // THE INFORMATIVE HALF OF THE PAIR, not both halves squeezed to nothing.
        // The market is one side of most rows — it is the counterparty of last
        // resort on three of the four clearing paths — so "Market > CalorVec..."
        // spends the cell on the constant and elides the name that varies.
        // Naming the OTHER side, with the direction the goods moved, puts the
        // whole cell behind the part that differs between rows. A corp-to-corp
        // exchange (the matched order-book path) still shows both, since neither
        // side is redundant there.
        //
        // Direction is from the GOOD's point of view: it left the seller and
        // reached the buyer.
        ImGui::TableSetColumnIndex(2);
        std::string with;
        if (r.seller_is_market && !r.buyer_is_market)
            with = "to " + short_corp_name(r.buyer);
        else if (r.buyer_is_market && !r.seller_is_market)
            with = "from " + short_corp_name(r.seller);
        else
            with = short_corp_name(r.seller) + " > " + short_corp_name(r.buyer);
        ui::fit_text(ui::text_box::table_cell, "market.trades.between", with.c_str(),
                     ImGui::GetContentRegionAvail().x);
        if (ImGui::BeginItemTooltip())
        {
            ImGui::Text("Seller %s", r.seller.c_str());
            ImGui::Text("Buyer  %s", r.buyer.c_str());
            ImGui::EndTooltip();
        }

        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted(ui::fmt::abbreviate(static_cast<double>(r.revenue)).c_str());

        if (ImGui::BeginItemTooltip())
        {
            ImGui::Text("%s, qtr %d", r.name, r.tick);
            ImGui::Separator();
            ImGui::Text("Seller   %s", r.seller.c_str());
            ImGui::Text("Buyer    %s", r.buyer.c_str());
            ImGui::Text("Quantity %.1f", static_cast<double>(r.quantity));
            ImGui::Text("Unit     %.2f", static_cast<double>(r.unit_price));
            ImGui::Text("Revenue  %.0f", static_cast<double>(r.revenue));
            ImGui::TextDisabled("Revenue, not margin: no cost basis exists.");
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

/// The Trades tab. Four headed sections in one scroller, and the heads are the
/// point: the three reads cost different things to know and the surface says so.
void draw_trades_tab(world& w, const recipe_registry& reg, ui_state& state,
                     entity_id body, entity_id mid, const market_component& mc)
{
    const entity_id corp = w.player_entity;
    g_trades_market = mid;

    // Reads 1 and 2 are cheap filters and refresh every frame — a press must be
    // reflected the frame after it applies.
    g_my_trades          = collect_trades(w, body, true);
    g_market_trades_open = player_operates_on_body(w, body);
    g_market_trades      = g_market_trades_open ? collect_trades(w, body, false)
                                               : std::vector<trade_row_record>{};

    // READ 3 AND THE HISTORY ARE CACHED AGAINST THE ECON TICK, and that is a
    // performance requirement rather than a nicety. Pricing a leg runs a
    // terrain-weighted A* on a cache miss; doing that per frame over every market
    // is the shape of the AppHangB1 stall that narrowed
    // `invalidate_logistics_caches` in the first place. Nothing either read
    // reports can change between econ ticks — prices, the book and the exchange
    // ring all move on the clearing tick — so a per-tick refresh is not a
    // staleness compromise, it is the actual update rate of the data.
    //
    // MEASURED with the tab left open across twelve econ ticks (Debug build,
    // `frame_csv`): mean frame BUILD 78.06 ms closed against 81.93 ms open, worst
    // 91.18 against 97.84. So the recompute costs about 4 ms of mean build and
    // under 7 ms at the worst tick — bounded, because the gross-spread test prunes
    // before the pricing call and the A* cache is warm after the first pair.
    // The estate is part of the key because a BUILD OR A DEMOLITION moves the
    // haulage origin (`corp_representative_tile`) and can open or shut a lane
    // outright, and both are presses — they land between econ ticks, so a key of
    // tick alone would leave the derivation stale for the rest of the quarter
    // after the player changed the thing it depends on.
    const auto pcit = w.corporations.find(corp);
    const std::size_t assets =
        (pcit != w.corporations.end()) ? pcit->second.assets.size() : 0;

    if (g_cache_market != mid || g_cache_tick != w.current_econ_tick
        || g_cache_markets != w.markets.size() || g_cache_assets != assets)
    {
        g_cache_market  = mid;
        g_cache_tick    = w.current_econ_tick;
        g_cache_markets = w.markets.size();
        g_cache_assets  = assets;
        g_potential     = derive_potential_trades(w, reg, body, mid);
        g_exchanges     = derive_exchange_rows(w, mid);
    }

    // Named child so `verify.scroll_panel("market_trades", ...)` reaches the REAL
    // scroller rather than the window, which has no scrollable extent (NR-719).
    // A NAME OF ITS OWN, not the Goods child's: a tab strip's two views are two
    // different scrollers and only one is on screen, so one name for both would
    // aim the request at whichever happened to be up.
    if (ImGui::BeginChild("##trades_scroll", {0.0f, 0.0f}, false))
    {
        ui::foldout_scroll_child("##trades_scroll");

        // EACH LONG SECTION IS BOUNDED AND SCROLLS INSIDE ITSELF, and that is the
        // design's requirement rather than a layout preference. The book here
        // runs to 24 rows on the shipped fixture and the exchange read to 120;
        // laid out end to end the first of them fills the column and the other
        // three reads are below the fold on open. A tab whose headline question
        // is "what could I be doing?" cannot open on a list of rival orders with
        // the answer three screens down. Bounding each section keeps all four
        // HEADS on screen — which is what "kept visibly distinct" has to mean on
        // a 380 px column — and the depth is one scroll inside the section that
        // has it.
        // MEASURED against the column at 1920x1080: the content area runs about
        // 595 px and a line is about 20, so four heads plus their notes plus
        // these four tables come to ~630 px. Deliberately a little over — the
        // outer scroller has to have somewhere to go, or a "foot" capture is the
        // head again and NR-719 repeats itself by a third route.
        constexpr int k_mine_cap = 5;
        constexpr int k_book_cap = 5;
        constexpr int k_pot_cap  = 7; // the headline read gets the most
        constexpr int k_hist_cap = 6;

        // --- READ 1: my standing trades ---------------------------------------
        ImGui::SeparatorText("My trades");
        if (g_my_trades.empty())
        {
            ImGui::TextDisabled("No standing trades on this body.");
        }
        else if (static_cast<int>(g_my_trades.size()) <= k_mine_cap)
        {
            draw_trade_table("##mine", g_my_trades, false, true, state, corp);
        }
        else
        {
            ImGui::BeginChild("##mine_box", {0.0f, table_height(k_mine_cap)}, false);
            draw_trade_table("##mine", g_my_trades, false, true, state, corp);
            ImGui::EndChild();
        }

        if (ImGui::TreeNode("Place a trade"))
        {
            draw_place_order_form(state, corp, body, mc);
            ImGui::TreePop();
        }

        // --- READ 2: the market's standing trades -----------------------------
        ImGui::SeparatorText("All trades here");
        if (!g_market_trades_open)
        {
            // THE GATE, STATED. A shut gate and an empty book are different
            // answers and the surface must not collapse them.
            ImGui::TextDisabled("You hold no building on %s.", body_label(w, body).c_str());
            ImGui::TextDisabled("The book is readable where you operate.");
        }
        else if (g_market_trades.empty())
        {
            ImGui::TextDisabled("Nothing standing on this body.");
        }
        else
        {
            ImGui::BeginChild("##all_box", {0.0f, table_height(k_book_cap)}, false);
            draw_trade_table("##all", g_market_trades, true, false, state, corp);
            ImGui::EndChild();
            ImGui::TextDisabled("%d standing, mine included.",
                                static_cast<int>(g_market_trades.size()));
        }

        // --- READ 3: potential trades -----------------------------------------
        ImGui::SeparatorText("Potential trades");
        if (g_potential.empty())
        {
            ImGui::TextDisabled("No route from here clears its haulage.");
        }
        else
        {
            ImGui::TextDisabled("Buy here, sell there, less haulage. Per unit.");
            ImGui::BeginChild("##pot_box", {0.0f, table_height(k_pot_cap)}, false);
            draw_potential_table(g_potential);
            ImGui::EndChild();
        }

        // --- The history half --------------------------------------------------
        ImGui::SeparatorText("Recent trades");
        if (g_exchanges.empty())
        {
            ImGui::TextDisabled("Nothing has cleared here yet.");
        }
        else
        {
            ImGui::TextDisabled("Revenue, not margin: no cost basis exists.");
            ImGui::BeginChild("##hist_box", {0.0f, table_height(k_hist_cap)}, false);
            draw_history_table(g_exchanges);
            ImGui::EndChild();
        }
    }
    ImGui::EndChild();
}

// --- The nation presence row (BL-688) ----------------------------------------
// "Which nations operate in this market?" — a wrapped row of one chip per
// nation, sitting BELOW the Body/Market combos and ABOVE the tab strip, which is
// where Ben placed it (2026-08-29) and is part of the spec rather than a layout
// convenience: it qualifies the market the selectors just chose, before the tabs
// ask a question about it.
//
// THESE ARE NOT FLAGS, and the distinction is deliberate. `nation_colour` is a
// palette entry and is ALL that exists — corporations carry an emblem tag,
// nations do not. Real per-nation emblem artwork would be a generated identity
// system, a feature in its own right, so a single stubbed glyph standing in for
// one would teach a vocabulary the game does not have. A colour chip carrying
// the nation's initials is honest about being an identity mark and nothing more
// (ICONS.md § 2b), and it is a PLACEHOLDER even at that (Ben, 2026-08-29: flags
// and glyphs are a later sprint).

/// The nations operating in `mid`, by ascending entity id (deterministic).
///
/// DERIVED, because no store answers it. "Activity in a market" is grounded on
/// the two things that do exist: a building sits on a tile, and `market_for_tile`
/// resolves that tile's catchment market (the same routing `clear_markets` uses,
/// so a nation appears here exactly when its ground actually clears here). The
/// tile's owning nation comes from `tile_to_nation`. No invention, and nothing
/// that could disagree with where the goods really go.
std::vector<entity_id> nations_in_market(const world& w, entity_id mid)
{
    std::vector<entity_id> out;
    for (const auto& [bid, b] : w.buildings)
    {
        (void)bid;
        // `w.buildings` is keyed by BUILDING id; the tile is a field on the
        // component. Reading the key as a tile silently matched nothing, and the
        // row rendered "No national presence." on every market.
        const entity_id tile = b.tile;
        if (tile == null_entity || market_for_tile(w, tile) != mid)
            continue;
        const auto nit = w.tile_to_nation.find(tile);
        if (nit == w.tile_to_nation.end() || nit->second == null_entity)
            continue;
        if (std::find(out.begin(), out.end(), nit->second) == out.end())
            out.push_back(nit->second);
    }
    std::sort(out.begin(), out.end());
    return out;
}

/// Up to two initials for a nation name — "Kua Sua" -> "KS", "Huhaidar" -> "HU".
/// Two characters is what fits a chip at this column width; a single letter
/// collides far too often across a generated roster.
void nation_initials(const world& w, entity_id nid, char* out, std::size_t n)
{
    out[0] = '\0';
    const auto it = w.nations.find(nid);
    if (it == w.nations.end() || it->second.name.empty())
    {
        std::snprintf(out, n, "??");
        return;
    }
    const std::string& name = it->second.name;
    char a = '\0', b = '\0';
    bool at_word_start = true;
    for (const char c : name)
    {
        if (c == ' ' || c == '-' || c == '\'') { at_word_start = true; continue; }
        if (at_word_start)
        {
            if (a == '\0')      a = c;
            else if (b == '\0') { b = c; break; }
            at_word_start = false;
        }
    }
    // A single-word name gives its first TWO letters rather than one, so
    // "Huhaidar" reads "HU" and not "H".
    if (a != '\0' && b == '\0' && name.size() > 1)
        for (const char c : name)
            if (c != a && c != ' ') { b = c; break; }
    if (a == '\0') { std::snprintf(out, n, "??"); return; }
    const char up_a = static_cast<char>(std::toupper(static_cast<unsigned char>(a)));
    if (b != '\0')
    {
        const char up_b = static_cast<char>(std::toupper(static_cast<unsigned char>(b)));
        std::snprintf(out, n, "%c%c", up_a, up_b);
    }
    else
        std::snprintf(out, n, "%c", up_a);
}

std::vector<nation_chip_record> g_nation_chips;

void draw_nation_presence_row(const world& w, entity_id mid)
{
    const std::vector<entity_id> nations = nations_in_market(w, mid);
    g_nation_chips.clear();
    for (const entity_id nid : nations)
    {
        nation_chip_record rec;
        rec.nation = nid;
        const auto nit = w.nations.find(nid);
        rec.name = (nit != w.nations.end()) ? nit->second.name : std::string{};
        char ini[8];
        nation_initials(w, nid, ini, sizeof ini);
        rec.initials = ini;
        g_nation_chips.push_back(rec);
    }
    if (nations.empty())
    {
        ImGui::TextDisabled("No national presence.");
        return;
    }

    const float avail   = ImGui::GetContentRegionAvail().x;
    const float start_x = ImGui::GetCursorPosX();
    const float chip_w  = ImGui::GetFontSize() * 2.0f;
    const float chip_h  = ImGui::GetFontSize() + 4.0f;
    const float gap     = 4.0f;

    for (std::size_t i = 0; i < nations.size(); ++i)
    {
        // Wrap to a second or third row when the run reaches the column edge —
        // "wrapping to 2 or 3 rows if needed" (Ben). At the current nation count
        // that is unlikely to trigger, but the row must not be the surface that
        // clips when a denser world does trigger it (NR-709's family).
        if (i > 0)
        {
            const float next_x = ImGui::GetCursorPosX() + chip_w + gap;
            if (next_x - start_x <= avail)
                ImGui::SameLine(0.0f, gap);
        }

        const entity_id nid = nations[i];
        char ini[8];
        nation_initials(w, nid, ini, sizeof ini);

        const ImU32  col = palette::nation_colour(nid);
        const ImVec2 p0  = ImGui::GetCursorScreenPos();
        ImDrawList*  dl  = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p0, {p0.x + chip_w, p0.y + chip_h}, col, 3.0f);

        // Initials in whichever of black/white actually reads on this chip —
        // the nation palette spans light and dark, so one fixed ink would be
        // illegible on roughly half the roster.
        const unsigned cr = (col >> IM_COL32_R_SHIFT) & 0xFFu;
        const unsigned cg = (col >> IM_COL32_G_SHIFT) & 0xFFu;
        const unsigned cb = (col >> IM_COL32_B_SHIFT) & 0xFFu;
        const float lum = (0.299f * static_cast<float>(cr) + 0.587f * static_cast<float>(cg)
                           + 0.114f * static_cast<float>(cb)) / 255.0f;
        const ImU32 ink = (lum > 0.55f) ? IM_COL32(20, 22, 28, 255) : IM_COL32(245, 247, 250, 255);

        const ImVec2 ts = ImGui::CalcTextSize(ini);
        dl->AddText({p0.x + (chip_w - ts.x) * 0.5f, p0.y + (chip_h - ts.y) * 0.5f}, ink, ini);

        // The chip is an InvisibleButton so the NAME is one hover away. A
        // two-letter mark is not self-explanatory and is not meant to be; the
        // tooltip is where the identity actually lives.
        const std::string chip_id = "##nat" + std::to_string(nid);
        ImGui::InvisibleButton(chip_id.c_str(), {chip_w, chip_h});
        if (ImGui::BeginItemTooltip())
        {
            const auto nit = w.nations.find(nid);
            ImGui::TextUnformatted(nit != w.nations.end() && !nit->second.name.empty()
                                   ? nit->second.name.c_str() : "Unknown nation");
            ImGui::EndTooltip();
        }
    }
}

// --- The Goods table (BL-686) -------------------------------------------------
// "What is each good worth here, and which way is it moving?" ONE ROW PER TRADED
// GOOD: item glyph, name, price, body average price, price vs base, and an
// 8-quarter graph flattened INTO the row rather than stacked under it.
//
// WHAT IT REPLACES, AND WHY DENSITY WAS THE SMALLEST OF THE THREE PROBLEMS. The
// old view gave each good a name line plus a 44 px chart — ~72 px of column, so
// 3.5 goods at 1280x720 and 9 at 1920x1080 against a roster of ~42. But:
//
//   - EVERY SPARKLINE HAD ITS OWN IMPLICIT SCALE. `draw_plot` fits each series to
//     its own min/max, so a good decaying 40% and a good flat to within a
//     rounding error drew the same shape. Comparing goods is the one thing a
//     price board is for, and it was the one thing this could not do.
//   - THERE WAS NO DIRECTION. "now 0.63" says nothing about which way it is
//     going, and the curve had no axis to read it against.
//   - NOTHING HAD EVER SEEN PAST THE FOURTH GOOD (NR-719). The scroll hook aimed
//     at the window while the list sat in a child scroller, so the "foot" capture
//     was byte-identical to the head. Fixed in foldout_column/verify_api; the
//     child here is named so the request can reach it.
//
// THE SHARED SCALE IS `price / base_price`. That is the honest common axis for a
// board whose goods differ by orders of magnitude in absolute price: 1.0 is the
// good's own base, so every sparkline is read against the same horizontal
// reference and two goods' movements are finally comparable. It is also exactly
// what the `vs base` column reports, so the number and the picture cannot
// disagree. DIRECTION is carried by colour — the line takes the rising or falling
// tint from its net movement across the window — and by the baseline itself.
//
// THE WINDOW IS 8 QUARTERS, NOT SIX MONTHS (Ben, 2026-08-29, once the arithmetic
// was put to him). `record_histories` samples once per econ tick and a tick is a
// calendar quarter, so "six months" would be two points. 8 of the 64 retained.

/// How many trailing samples the row graph draws. See above: quarters, not months.
constexpr std::size_t k_graph_quarters = 8;

/// Mean `price[r]` across every market on `body` — the one new derivation the
/// Goods table needs (`body_average_price`). A walk the surface does itself.
float body_average_price(const world& w, entity_id body, std::size_t r)
{
    float sum = 0.0f;
    int   n   = 0;
    for (const auto& [mid, mc] : w.markets)
    {
        (void)mid;
        if (mc.body != body || mc.base_price[r] <= 0.0f)
            continue;
        sum += mc.price[r];
        ++n;
    }
    return (n > 0) ? sum / static_cast<float>(n) : 0.0f;
}

/// The row's inline graph: the trailing `k_graph_quarters` samples of
/// `price / base` drawn against a 1.0 baseline, on a scale SHARED by every row.
///
/// Hand-drawn rather than `draw_plot` precisely because draw_plot's per-series
/// autoscale is the defect being fixed. The vertical span is handed in by the
/// caller, computed once over the whole table, so a row that barely moves LOOKS
/// like a row that barely moves.
void draw_row_graph(const std::vector<float>& price_series, float base,
                    ImVec2 size, float lo, float hi)
{
    ImDrawList*  dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::Dummy(size);

    const ImU32 frame_col = IM_COL32(70, 76, 92, 255);
    const ImU32 base_col  = IM_COL32(120, 126, 142, 200);

    // The 1.0 reference — the good's own base price. Every row draws it at the
    // same height, which is what makes the rows comparable at a glance.
    const auto y_of = [&](float ratio) {
        const float t = (ratio - lo) / (hi - lo);
        const float c = (t < 0.0f) ? 0.0f : (t > 1.0f ? 1.0f : t);
        return p0.y + size.y * (1.0f - c);
    };
    dl->AddLine({p0.x, y_of(1.0f)}, {p0.x + size.x, y_of(1.0f)}, base_col, 1.0f);

    if (base <= 0.0f || price_series.size() < 2)
    {
        dl->AddRect(p0, {p0.x + size.x, p0.y + size.y}, frame_col, 2.0f);
        return;
    }

    const std::size_t n     = std::min(k_graph_quarters, price_series.size());
    const std::size_t first = price_series.size() - n;

    // Direction over the window, which is what the colour reports.
    const float r_first = price_series[first] / base;
    const float r_last  = price_series.back() / base;
    const ImU32 line_col =
        (r_last > r_first * 1.005f) ? IM_COL32(90, 200, 140, 255) :  // rising
        (r_last < r_first * 0.995f) ? IM_COL32(225, 120, 110, 255) : // falling
                                      IM_COL32(150, 158, 178, 255);  // flat

    ImVec2 prev{};
    for (std::size_t i = 0; i < n; ++i)
    {
        const float t = (n == 1) ? 0.0f : static_cast<float>(i) / static_cast<float>(n - 1);
        const ImVec2 pt{p0.x + size.x * t, y_of(price_series[first + i] / base)};
        if (i > 0)
            dl->AddLine(prev, pt, line_col, 1.6f);
        prev = pt;
    }
    // The newest sample gets a dot: on an 8-point line the "which end is now"
    // question is otherwise a guess.
    dl->AddCircleFilled(prev, 2.0f, line_col);
    dl->AddRect(p0, {p0.x + size.x, p0.y + size.y}, frame_col, 2.0f);
}

/// The placeholder item glyph.
///
/// PLACEHOLDER BY INSTRUCTION (Ben, 2026-08-29: flags and glyphs are a later
/// sprint). The settled design is four value-track silhouettes (`ICONS.md` § 2b)
/// and this is not one of them. Two conditions come with the stand-in and both
/// are load-bearing:
///
///  1. IT MUST LOOK PROVISIONAL, not be learned as meaningful. So it is a dashed
///     outline square in the resource's identity colour — legibly a slot waiting
///     for artwork, and deliberately NOT a shape that could be mistaken for one
///     of the four tracks. The colour carries the only real identity it has.
///  2. IT RESERVES THE REAL GLYPH'S WIDTH, so landing the real glyph is an
///     icons.cpp change and not a second table layout.
void draw_item_glyph_placeholder(ImU32 colour, float box)
{
    ImDrawList*  dl = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::Dummy({box, box});

    const float  pad = box * 0.18f;
    const ImVec2 a{p0.x + pad, p0.y + pad};
    const ImVec2 b{p0.x + box - pad, p0.y + box - pad};
    // A dashed square: short strokes per side, so it reads as "unfinished"
    // rather than as a solid mark with meaning.
    const float  seg = (b.x - a.x) / 5.0f;
    for (int i = 0; i < 5; i += 2)
    {
        const float x = a.x + seg * static_cast<float>(i);
        dl->AddLine({x, a.y}, {x + seg, a.y}, colour, 1.0f);
        dl->AddLine({x, b.y}, {x + seg, b.y}, colour, 1.0f);
        const float y = a.y + seg * static_cast<float>(i);
        dl->AddLine({a.x, y}, {a.x, y + seg}, colour, 1.0f);
        dl->AddLine({b.x, y}, {b.x, y + seg}, colour, 1.0f);
    }
}

/// The Goods table's drawn rows, refreshed every frame the table draws. See
/// `goods_row_record` in the header for why the check reads what was DRAWN.
std::vector<goods_row_record> g_goods_rows;
entity_id                     g_goods_market = null_entity;

void draw_goods_tab(const world& w, ui_state& s, entity_id body,
                    entity_id mid, const market_component& mc,
                    const market_plot_history& history)
{
    const auto hist_it = history.find(mid);

    g_goods_rows.clear();
    g_goods_market = mid;

    // ROW HEIGHT IS THE OPEN MEASUREMENT (Ben, 2026-08-29: "let's compare this at
    // 8 rows, 10 rows, and 12 rows"). It is a ui_state dial rather than a
    // constant so the three variants are one capture script and not three builds,
    // and so the answer can be set without a recompile.
    const int   rows_target = (s.market_goods_rows > 0) ? s.market_goods_rows : 10;
    const float avail_h     = ImGui::GetContentRegionAvail().y;
    const float row_h       = (avail_h > 0.0f)
                            ? avail_h / static_cast<float>(rows_target)
                            : ImGui::GetFrameHeight();

    // THE SHARED VERTICAL SPAN — SYMMETRIC ABOUT BASE, AND ZOOMED (Ben,
    // 2026-08-29: "we can fix it by centring on the body base price, and zooming
    // in").
    //
    // The first cut spanned the global min and max of `price / base` across every
    // good, which is honest and unreadable: the fixture runs 0.25x to 2.77x, so a
    // good moving 1.00 -> 1.05 got 1.7% of the cell height and forty rows drew as
    // empty boxes. One outlier set the scale for everyone.
    //
    // Two properties replace it, and the first is what keeps the rows comparable:
    //
    //  * SYMMETRIC about 1.0, so the baseline is the exact vertical centre of
    //    every cell and "at base" means the same height on every row. That is the
    //    comparability the shared span was for; it does not depend on the rows
    //    sharing a numeric range, only a shared MEANING for the midline.
    //  * ZOOMED to a robust spread rather than the extremes. The half-width is the
    //    75th percentile of |ratio - 1| over every drawn sample, so at least three
    //    quarters of the data is on-scale and the outliers clip instead of
    //    flattening everything else.
    //
    // A clipped sample pins to the cell edge (`y_of` clamps), which reads as "off
    // the scale" rather than as a value — and the `v.Base` column carries the
    // exact ratio on the same row, so nothing is actually lost by clipping it.
    std::vector<float> dev;
    if (hist_it != history.end())
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (mc.base_price[r] <= 0.0f)
                continue;
            const std::vector<float>& ser = hist_it->second[r].price;
            if (ser.empty())
                continue;
            const std::size_t n     = std::min(k_graph_quarters, ser.size());
            const std::size_t first = ser.size() - n;
            for (std::size_t i = 0; i < n; ++i)
                dev.push_back(std::fabs(ser[first + i] / mc.base_price[r] - 1.0f));
        }

    // p75, and a floor so a market that has barely moved does not zoom into its
    // own noise and report a flat quarter as a crisis.
    float half = 0.05f;
    if (!dev.empty())
    {
        const std::size_t k = (dev.size() * 3) / 4;
        std::nth_element(dev.begin(), dev.begin() + static_cast<std::ptrdiff_t>(k), dev.end());
        half = std::max(0.05f, dev[k]);
    }
    const float lo = 1.0f - half;
    const float hi = 1.0f + half;

    // Named child so `verify.scroll_panel("market", …)` can reach the REAL
    // scroller (NR-719): the request names this key, not the window.
    if (ImGui::BeginChild("##goods_scroll", {0.0f, 0.0f}, false))
    {
        ui::foldout_scroll_child("##goods_scroll");

        constexpr ImGuiTableFlags flags =
            ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;

        // `body_average_price` is the column the design nominated as first to
        // drop IF the row will not fit — measured, not assumed (see
        // `market_goods_show_body`). Both layouts are built so the pictures can
        // decide, which is what the brief asked for.
        const bool  show_body = s.market_goods_show_body;
        const int   n_cols    = show_body ? 6 : 5;

        if (ImGui::BeginTable("##goods", n_cols, flags))
        {
            const float em = ImGui::GetFontSize();
            // FIXED SIBLINGS SIZED IN em, AND THE NAME TAKES WHAT IS LEFT. Budget
            // against the font, never against a pixel count: the column host is
            // ~380 px at 1280 and 384 px at 1920 (`shell_column_width`), and the
            // difference between those resolutions is all VERTICAL. NR-709 is the
            // failure this avoids — a WidthStretch name column behind fixed
            // siblings that ate it entirely, on four surfaces so far.
            // Sized to the WIDEST STRING EACH COLUMN CAN HOLD, measured at the
            // live font, rather than to an em multiple guessed at authoring time.
            // The first cut used multiples and they were tuned against a 13 px
            // font while the shell renders at ~18 px, so the fixed siblings ate
            // the name column and "Iron Ore" and "Iron-something" both drew as
            // "Iron ..." — two goods, one string, on a board whose whole job is
            // telling goods apart. Measuring is the only version of this that
            // cannot go stale when the font changes.
            const float pad     = ImGui::GetStyle().CellPadding.x * 2.0f;
            const float w_price = ImGui::CalcTextSize("00.00").x + pad;
            const float w_vbase = ImGui::CalcTextSize("0.00x").x + pad;
            const float w_graph = em * 3.4f;

            ImGui::TableSetupColumn("##g",    ImGuiTableColumnFlags_WidthFixed, em * 0.9f);
            ImGui::TableSetupColumn("Good",   ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Price",  ImGuiTableColumnFlags_WidthFixed, w_price);
            if (show_body)
                ImGui::TableSetupColumn("Body", ImGuiTableColumnFlags_WidthFixed, w_price);
            ImGui::TableSetupColumn("v.Base", ImGuiTableColumnFlags_WidthFixed, w_vbase);
            ImGui::TableSetupColumn("8 qtr",  ImGuiTableColumnFlags_WidthFixed, w_graph);
            ImGui::TableHeadersRow();

            bool any = false;
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                if (mc.base_price[r] <= 0.0f)
                    continue; // good not traded at this market
                any = true;
                ImGui::PushID(static_cast<int>(r));
                ImGui::TableNextRow(ImGuiTableRowFlags_None, row_h);

                const resource_presentation& rp =
                    presentation_of(static_cast<resource_type>(r));

                ImGui::TableSetColumnIndex(0);
                draw_item_glyph_placeholder(rp.colour, em * 0.95f);

                ImGui::TableSetColumnIndex(1);
                const float name_avail  = ImGui::GetContentRegionAvail().x;
                const float name_needed = ImGui::CalcTextSize(rp.name).x;
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(rp.colour));
                ui::fit_text(ui::text_box::table_cell, "market.goods.name", rp.name, name_avail);
                ImGui::PopStyleColor();

                int col = 2;
                ImGui::TableSetColumnIndex(col++);
                ImGui::Text("%.2f", static_cast<double>(mc.price[r]));

                const float body_avg = body_average_price(w, body, r);
                if (show_body)
                {
                    ImGui::TableSetColumnIndex(col++);
                    ImGui::TextDisabled("%.2f", static_cast<double>(body_avg));
                }

                ImGui::TableSetColumnIndex(col++);
                const float vs_base = mc.price[r] / mc.base_price[r];
                // The number carries direction too, in the same tints the graph
                // uses, so colour means one thing across the whole row.
                const ImU32 vb_col =
                    (vs_base > 1.005f) ? IM_COL32(90, 200, 140, 255) :
                    (vs_base < 0.995f) ? IM_COL32(225, 120, 110, 255) :
                                         IM_COL32(150, 158, 178, 255);
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(vb_col),
                                   "%.2fx", static_cast<double>(vs_base));

                ImGui::TableSetColumnIndex(col);
                const float gw = ImGui::GetContentRegionAvail().x;
                const float gh = std::max(8.0f, row_h - 4.0f);
                int samples = 0;
                if (hist_it != history.end())
                {
                    const std::vector<float>& ser = hist_it->second[r].price;
                    samples = static_cast<int>(std::min(k_graph_quarters, ser.size()));
                    draw_row_graph(ser, mc.base_price[r], {gw, gh}, lo, hi);
                }
                else
                    ImGui::TextDisabled("-");

                goods_row_record rec;
                rec.resource   = static_cast<resource_type>(r);
                rec.name       = rp.name;
                rec.price      = mc.price[r];
                rec.base_price = mc.base_price[r];
                rec.body_avg   = body_avg;
                rec.vs_base    = vs_base;
                rec.samples     = samples;
                rec.name_avail  = name_avail;
                rec.name_needed = name_needed;
                g_goods_rows.push_back(rec);

                ImGui::PopID();
            }
            ImGui::EndTable();

            if (!any)
                ImGui::TextDisabled("No tradeable goods at this market.");
        }
    }
    ImGui::EndChild();
}

} // namespace

const std::vector<goods_row_record>&   goods_rows()   { return g_goods_rows; }
entity_id                              goods_market() { return g_goods_market; }
const std::vector<nation_chip_record>& nation_chips() { return g_nation_chips; }

const std::vector<trade_row_record>&       my_trades()          { return g_my_trades; }
const std::vector<trade_row_record>&       market_trades()      { return g_market_trades; }
bool                                       market_trades_open() { return g_market_trades_open; }
const std::vector<potential_trade_record>& potential_trades()   { return g_potential; }
const std::vector<exchange_row_record>&    exchange_rows()      { return g_exchanges; }
entity_id                                  trades_market()      { return g_trades_market; }

void draw_market_ledger(world& w, const recipe_registry& reg, ui_state& s,
                        const market_plot_history& history, bool& open)
{
    if (!open)
        return;

    // Re-hosted into the shell fold-out column (BL-122); closed via the nav rail.
    if (!ui::foldout_begin("Market Ledger"))
    {
        ui::foldout_end();
        return;
    }

    if (w.markets.empty())
    {
        ImGui::TextDisabled("No markets.");
        ui::foldout_end();
        return;
    }

    // --- Body selector ---
    // Collect distinct bodies that have at least one market.
    std::vector<entity_id> bodies;
    for (const auto& [mid, mc] : w.markets)
    {
        if (std::find(bodies.begin(), bodies.end(), mc.body) == bodies.end())
            bodies.push_back(mc.body);
    }
    std::sort(bodies.begin(), bodies.end());

    static entity_id selected_body        = null_entity;
    static entity_id last_seen_selection  = null_entity;

    // Selecting a market elsewhere (canvas click / Selection element) routes
    // here (BL-159): when the player's current selection is a market that
    // differs from the one we last picked up, jump the body/market selectors
    // to it. `last_seen_selection` guards this so the player can still browse
    // away with the combos afterward without being yanked back every frame.
    entity_id pending_focus_market = null_entity;
    if (s.selected_entity != last_seen_selection)
    {
        last_seen_selection = s.selected_entity;
        const auto fit = w.markets.find(s.selected_entity);
        if (fit != w.markets.end())
        {
            selected_body        = fit->second.body;
            pending_focus_market = s.selected_entity;
        }
    }

    if (selected_body == null_entity || w.bodies.find(selected_body) == w.bodies.end())
        selected_body = (w.home_body != null_entity && w.bodies.count(w.home_body))
                        ? w.home_body : bodies.front();

    const std::string body_preview = body_label(w, selected_body);
    if (ImGui::BeginCombo("Body", body_preview.c_str()))
    {
        for (entity_id bid : bodies)
        {
            const bool sel = (bid == selected_body);
            if (ImGui::Selectable(body_label(w, bid).c_str(), sel))
                selected_body = bid;
            if (sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    const std::vector<entity_id> body_markets = markets_on_body(w, selected_body);
    if (body_markets.empty())
    {
        ImGui::TextDisabled("No markets on this body.");
        ui::foldout_end();
        return;
    }

    // --- Market / city selector (cascades from the body) ---
    static entity_id selected_market = null_entity;
    if (pending_focus_market != null_entity &&
        std::find(body_markets.begin(), body_markets.end(), pending_focus_market) != body_markets.end())
    {
        selected_market = pending_focus_market;
    }
    pending_focus_market = null_entity;
    if (selected_market == null_entity ||
        std::find(body_markets.begin(), body_markets.end(), selected_market) == body_markets.end())
        selected_market = body_markets.front();

    const std::string mkt_preview = market_city_name(w, selected_market);
    if (ImGui::BeginCombo("Market", mkt_preview.c_str()))
    {
        for (entity_id mid : body_markets)
        {
            const bool sel = (mid == selected_market);
            if (ImGui::Selectable(market_city_name(w, mid).c_str(), sel))
                selected_market = mid;
            if (sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    if (w.markets.find(selected_market) == w.markets.end())
    {
        ui::foldout_end();
        return;
    }
    const market_component& mc = w.markets.at(selected_market);

    // --- The nation presence row (BL-688) -----------------------------------
    // BELOW the selectors, ABOVE the tab strip — Ben's placement, and part of the
    // spec: it qualifies the market the combos just chose, before the tabs ask a
    // question about it.
    draw_nation_presence_row(w, selected_market);
    ImGui::Separator();

    // --- View tabs -----------------------------------------------------------
    // GOODS (the reworked Prices view, BL-686) and TRADES. Convoys has LEFT for
    // its own rail slot (BL-689): it was never a market question, and the
    // flattening needed the column's full height.
    //
    // THE SECOND TAB IS "Trades", NOT "Sell Orders" (Ben, 2026-08-29). The word
    // carries the widening it was renamed for: a sell order is one direction and
    // one actor, a trade is a position either way round held by anyone in the
    // market — plus, now that the clearing tick retains a per-exchange record,
    // one that has already happened.
    ui::nav_button("Goods",  0, s.market_ledger_view, &open);
    ImGui::SameLine();
    ui::nav_button("Trades", 1, s.market_ledger_view, &open);
    // The old strip had a third button; `market_ledger_view` can still hold 2
    // from a save or a verify hook, so clamp rather than drawing nothing.
    if (s.market_ledger_view > 1)
        s.market_ledger_view = 0;
    ImGui::Separator();
    ImGui::Spacing();

    if (s.market_ledger_view == 1)
    {
        draw_trades_tab(w, reg, s, selected_body, selected_market, mc);
        ui::foldout_end();
        return;
    }
    // The Trades tab was not on screen; its records must not read as this frame's.
    clear_trade_records();

    draw_goods_tab(w, s, selected_body, selected_market, mc, history);

    ui::foldout_end();
}

} // namespace ui
