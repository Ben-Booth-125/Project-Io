#include "market_ledger.hpp"

#include "foldout_column.hpp" // shell fold-out column host (BL-122)
#include "icons.hpp"
#include "plot_history.hpp"
#include "presentation.hpp"
#include "text_fit.hpp"

#include "world/market_clearing.hpp" // market_for_tile — the nation presence row

#include <imgui.h>

#include <algorithm>
#include <cctype>  // std::toupper — the nation chip's initials
#include <cmath>
#include <cstdio>  // std::snprintf
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

// --- Sell orders (player) ----------------------------------------------------
// Relocated from the Construction/Building panel (BL-159 — "how do I sell what
// I make?" is a market question, so it now lives on the market surface rather
// than the building surface). Columns/actions carried over faithfully from the
// old draw_sell_orders_section (construction_panel.cpp, pre-BL-159).
//
// BL-293 (2026-08-07): the orders themselves now live in `world::sell_orders`,
// so this reads the world and ENQUEUES commands rather than mutating a UI vector.
// The press does the same thing the AI's command does because it is the same
// command — the tab composes a `corp_command` and `app::render` applies it
// through `apply_corp_command`.
void draw_sell_orders_tab(const world& w, ui_state& state, entity_id body,
                          const market_component* market)
{
    const entity_id corp = w.player_entity;

    bool any = false;
    for (const sell_order& o : w.sell_orders)
    {
        if (o.corp != corp || o.body != body)
            continue;
        any = true;
        // PushID on the ORDER ID, not the loop index: the id is what the remove
        // command names, and it is stable across the erase that a press causes.
        ImGui::PushID(static_cast<int>(o.id));
        ImGui::Text("%s  x%.0f  >= %.1f",
            resource_name(o.resource), o.quantity, o.floor_price);
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove"))
        {
            corp_command cmd;
            cmd.corp  = corp;
            cmd.verb  = corp_verb::remove_sell_order;
            cmd.order = o.id;
            state.pending_order_commands.push_back(cmd);
        }
        ImGui::PopID();
    }
    if (!any)
        ImGui::TextDisabled("No sell orders on this body.");

    if (market == nullptr)
    {
        ImGui::TextDisabled("This body has no market.");
        return;
    }

    ImGui::Separator();
    static int   add_resource = -1;
    static float add_quantity = 10.0f;
    static float add_floor    = 0.0f;

    // Reset the add-order form when the selected market's body changes — the
    // statics otherwise carry one market's resource/quantity onto another.
    static entity_id form_body = null_entity;
    if (form_body != body)
    {
        form_body    = body;
        add_resource = -1;
        add_quantity = 10.0f;
        add_floor    = 0.0f;
    }

    if (add_resource < 0 || market->base_price[static_cast<std::size_t>(add_resource)] <= 0.0f)
    {
        for (std::size_t r = 0; r < resource_count; ++r)
            if (market->base_price[r] > 0.0f) { add_resource = static_cast<int>(r); break; }
    }

    const char* preview = (add_resource >= 0)
        ? resource_name(static_cast<resource_type>(add_resource)) : "-";
    if (ImGui::BeginCombo("Resource", preview))
    {
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (market->base_price[r] <= 0.0f)
                continue;
            const bool sel = (add_resource == static_cast<int>(r));
            if (ImGui::Selectable(resource_name(static_cast<resource_type>(r)), sel))
                add_resource = static_cast<int>(r);
        }
        ImGui::EndCombo();
    }
    ImGui::InputFloat("Quantity / qtr", &add_quantity, 1.0f, 10.0f, "%.0f");
    ImGui::InputFloat("Floor price",     &add_floor,    0.1f, 1.0f,  "%.1f");
    if (add_quantity < 0.0f) add_quantity = 0.0f;
    if (add_floor    < 0.0f) add_floor    = 0.0f;

    ImGui::BeginDisabled(add_resource < 0 || add_quantity <= 0.0f);
    if (ImGui::Button("Add sell order"))
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

    // THE SHARED VERTICAL SPAN, computed once for the whole table. Every row's
    // graph is drawn against this, which is what "shared scale" means here — a
    // per-row span would just be the old autoscale wearing a table.
    float lo = 1.0f, hi = 1.0f;
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
            {
                const float ratio = ser[first + i] / mc.base_price[r];
                lo = std::min(lo, ratio);
                hi = std::max(hi, ratio);
            }
        }
    // A little headroom, and a guaranteed non-degenerate span.
    lo = std::min(lo, 0.95f);
    hi = std::max(hi, 1.05f);

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

void draw_market_ledger(const world& w, ui_state& s,
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
    // GOODS (the reworked Prices view, BL-686) and Sell Orders. Convoys has LEFT
    // for its own rail slot (BL-689): it was never a market question, and the
    // flattening needed the column's full height.
    //
    // Sell Orders keeps its current name deliberately. It becomes TRADES in a
    // later slice (BL-687), which waits on the exchange record `MARKETS.md`
    // § Trades says does not exist yet — clearing resolves a price and moves
    // quantity without pairing a buyer to a seller, so no realised margin is
    // recorded anywhere. Renaming the tab before the third read exists would
    // promise a column the world cannot fill.
    ui::nav_button("Goods",       0, s.market_ledger_view, &open);
    ImGui::SameLine();
    ui::nav_button("Sell Orders", 1, s.market_ledger_view, &open);
    // The old strip had a third button; `market_ledger_view` can still hold 2
    // from a save or a verify hook, so clamp rather than drawing nothing.
    if (s.market_ledger_view > 1)
        s.market_ledger_view = 0;
    ImGui::Separator();
    ImGui::Spacing();

    if (s.market_ledger_view == 1)
    {
        draw_sell_orders_tab(w, s, selected_body, &mc);
        ui::foldout_end();
        return;
    }

    draw_goods_tab(w, s, selected_body, selected_market, mc, history);

    ui::foldout_end();
}

} // namespace ui
