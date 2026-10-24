#include "header_panel.hpp"

#include "format.hpp"
#include "presentation.hpp"
#include "text_fit.hpp"

#include "world/economy_system.hpp" // BL-577: economy_report, for budgets[player].subsidies

#include <imgui.h>

#include <unordered_map>

namespace ui {

namespace {

/// BL-1406 (HEADER.md § Contents): the player's money is its NATION's treasury
/// once the world is folded — `purse_of(player_actor)` answers either owner, so
/// an unfolded world (a fixture) still reads the seat corporation's balance.
float player_treasury(const world& w)
{
    const float* p = purse_of(w, player_actor(w));
    return p != nullptr ? *p : 0.0f;
}

/// The last economy tick's net change to the player nation's treasury, read from
/// the nation's own row of the state ledger (`world::state_ledger`: the treasury
/// as the funding gate opened the tick, and as the budget settled it). False when
/// the nation holds no row this tick — no state industry ran — so the caller falls
/// back to the treasury series.
bool ledger_net(const world& w, float& net)
{
    const entity_id nation = player_nation(w);
    if (nation == null_entity) // BL-1407: every world is the nations'
        return false;
    const auto it = w.state_ledger.find(nation);
    if (it == w.state_ledger.end())
        return false;
    net = it->second.treasury_close - it->second.treasury_open;
    return true;
}

} // namespace

// Declared in header_panel.hpp; shared with the Budget ledger (BL-171) so both the
// header strip's CARGO figure and the ledger's Cargo Value use one valuation.
float player_cargo_value(const world& w)
{
    // BL-1265/BL-1269: no owner holds a stockpile any more — what it makes lands
    // on a market's shelf and is sold there (MARKETS.md § The shelf economy). The
    // goods the player still OWNS are its trades' cargo in transit, bought at the
    // source and sold on landing (TRADE.md § A trade), so that is what this
    // values: each in-flight convoy's cargo at its destination market's current
    // price — the price it will land into. BL-1406: the convoy's owner is the
    // player's ACTOR (its nation once folded); the seat corporation's id is kept
    // for a convoy dispatched before the fold.
    const entity_id actor = player_actor(w);
    float value = 0.0f;
    for (const convoy_component& c : w.convoys)
    {
        if ((c.corp != actor && c.corp != w.player_entity) || c.cargo_qty <= 0.0f)
            continue;
        const auto mit = w.markets.find(c.dest_market);
        if (mit == w.markets.end())
            continue;
        value += c.cargo_qty * mit->second.price[static_cast<std::size_t>(c.cargo_resource)];
    }
    return value;
}

void draw_header_panel(const world& w,
                       const std::vector<float>& balance_history,
                       const economy_report& report,
                       float left,
                       float right)
{
    const float width = right - left;
    if (width <= 0.0f)
        return; // window too narrow to show the strip

    // Top-align with the identity card (y = 0) so the two read as one level band.
    ImGui::SetNextWindowPos({left, 0.0f});
    ImGui::SetNextWindowSize({width, header_panel_height});

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoScrollbar         |
        ImGuiWindowFlags_NoNav               |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("##header_panel", nullptr, flags);

    // Vertically centre the single content row within the tall strip. Every element
    // on the row (text + sparkline) is drawn at one frame height, so centring the
    // start Y centres the whole row.
    ImGui::SetCursorPosY((header_panel_height - ImGui::GetFrameHeight()) * 0.5f);

    // BL-1406: the TREASURY of the player's nation (HEADER.md § Contents). A
    // nation never runs into debt, so the figure has no negative state and the
    // old `[in debt]` flag retires with the corporation balance it flagged.
    const float treasury  = player_treasury(w);
    const float valuation = player_cargo_value(w);

    // Last-tick net change: the nation's own state-ledger row (close less open);
    // with no row this tick, the most recent step of the treasury series.
    float net = 0.0f;
    if (!ledger_net(w, net) && balance_history.size() >= 2)
        net = balance_history.back() - balance_history[balance_history.size() - 2];

    // Guaranteed-fit strip (LAYOUT.md container vocabulary): the strip never wraps
    // to a second line. The three treasury/cargo/net figures are bounded-length
    // (fmt::credits/rate abbreviate to a handful of characters) and always drawn in
    // full; the sparkline is the genuine last resort to drop when the available
    // width is too narrow, so a load-bearing number is never silently clipped.
    const ImVec2 item_spacing = ImGui::GetStyle().ItemSpacing;
    const float  spark_width  = 96.0f;

    // BL-177 — the runway read: how long the treasury lasts at the current burn.
    // Shown only while it MEANS something — burning cash. A growing treasury shows
    // nothing rather than a lying "infinite quarters" figure. When the treasury
    // runs out a losing building idles (NATIONS.md § What a nation owns); the
    // nation does not borrow, so there is no "already underwater" state.
    // BL-577: this tick's contract income, named in the tooltip because a one-off
    // payment breaks the "assumes the burn holds steady" premise.
    float subsidies = 0.0f;
    if (const auto bit = report.budgets.find(player_actor(w)); bit != report.budgets.end())
        subsidies = bit->second.subsidies;

    std::string runway_text;
    std::string runway_tip;
    bool        runway_urgent = false;
    const bool  runway_meaningful = net < 0.0f;
    if (runway_meaningful)
    {
        const int quarters = static_cast<int>(std::max(0.0f, treasury) / -net);
        char      buf[32];
        std::snprintf(buf, sizeof(buf), "%dq", quarters);
        runway_text = buf;
        // Under two years of cushion is the point it needs acting on.
        runway_urgent = quarters < 8;
        char tip[320];
        if (subsidies > 0.0f)
            std::snprintf(tip, sizeof(tip),
                          "About %d quarters of cushion: treasury divided by the current\n"
                          "quarterly loss. Assumes the burn holds steady - it moves as\n"
                          "prices, wages and upkeep change. Includes Cr %.0f of contract\n"
                          "income this quarter - a one-off payment, not a steady rate.\n"
                          "When the treasury runs out, a losing building idles.",
                          quarters, static_cast<double>(subsidies));
        else
            std::snprintf(tip, sizeof(tip),
                          "About %d quarters of cushion: treasury divided by the current\n"
                          "quarterly loss. Assumes the burn holds steady - it moves as\n"
                          "prices, wages and upkeep change.\n"
                          "When the treasury runs out, a losing building idles.", quarters);
        runway_tip = tip;
    }

    float used_width = ImGui::CalcTextSize("TREASURY").x + item_spacing.x +
                       ImGui::CalcTextSize(fmt::credits(treasury).c_str()).x + item_spacing.x +
                       ImGui::CalcTextSize("   |   CARGO").x + item_spacing.x +
                       ImGui::CalcTextSize(fmt::credits(valuation).c_str()).x + item_spacing.x +
                       ImGui::CalcTextSize("   |   NET").x + item_spacing.x +
                       ImGui::CalcTextSize(fmt::rate(net, "qtr").c_str()).x;

    const float available  = ImGui::GetContentRegionAvail().x;
    bool        show_spark = balance_history.size() >= 2;

    // BL-177 runway fit, resolved BEFORE the decorative drop because the runway
    // is load-bearing and the sparkline is not. Container 4 (LAYOUT.md) forbids
    // silently truncating a header figure, so this degrades in three honest steps:
    //   full   "   |   RUNWAY  12q"   - label and value
    //   value  "   |   12q"           - label dropped, value kept (tooltip explains)
    //   none                          - folded into the NET tooltip instead
    const float runway_sep_w   = ImGui::CalcTextSize("   |   ").x;
    const float runway_label_w = ImGui::CalcTextSize("RUNWAY").x + item_spacing.x;
    const float runway_val_w   = ImGui::CalcTextSize(runway_text.c_str()).x;
    bool show_runway       = runway_meaningful;
    bool show_runway_label = false;
    if (show_runway)
    {
        if (used_width + runway_sep_w + runway_label_w + runway_val_w <= available)
        {
            show_runway_label = true;
            used_width += runway_sep_w + runway_label_w + runway_val_w;
        }
        else if (used_width + runway_sep_w + runway_val_w <= available)
        {
            used_width += runway_sep_w + runway_val_w;
        }
        else
        {
            show_runway = false; // no room at all — NET's tooltip carries it
        }
    }

    if (show_spark && used_width + item_spacing.x + spark_width > available)
        show_spark = false; // the sparkline is the last decorative element to drop

    // --- Treasury ---
    ImGui::TextDisabled("TREASURY");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Your nation's treasury. A nation never runs into debt.");
    ImGui::SameLine();
    {
        // The figures draw through fit_text (BL-215): the drop ladder above runs
        // first, so elision is the genuine last resort container 4 permits.
        // A nation's treasury is never negative; an unfolded fixture's seat
        // corporation balance can be, and is not shown in the positive colour.
        ImGui::PushStyleColor(ImGuiCol_Text, treasury < 0.0f ? palette::negative : palette::positive);
        fit_text(text_box::header_strip, "header.treasury", fmt::credits(treasury).c_str(),
                 ImGui::GetContentRegionAvail().x);
        ImGui::PopStyleColor();
    }

    // --- Cargo in transit, valued where it lands ---
    ImGui::SameLine();
    ImGui::TextDisabled("   |   CARGO");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Your trades' goods in transit, at the price where they land.");
    ImGui::SameLine();
    fit_text(text_box::header_strip, "header.cargo", fmt::credits(valuation).c_str(),
             ImGui::GetContentRegionAvail().x);

    // --- Last-tick net + a sparkline of recent treasuries ---
    ImGui::SameLine();
    ImGui::TextDisabled("   |   NET");
    ImGui::SameLine();
    {
        const ImU32 col = value_colour(fmt::sign_of(net));
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        fit_text(text_box::header_strip, "header.net", fmt::rate(net, "qtr").c_str(),
                 ImGui::GetContentRegionAvail().x);
        ImGui::PopStyleColor();
    }
    // NET carries the runway when there was no room to draw it (see the fit
    // resolution above), so the fact is never simply lost.
    if (runway_meaningful && !show_runway && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", runway_tip.c_str());

    // --- BL-177: runway — how long the treasury lasts at the current burn ---
    if (show_runway)
    {
        ImGui::SameLine();
        ImGui::TextDisabled(show_runway_label ? "   |   RUNWAY" : "   |  ");
        ImGui::SameLine();
        const ImU32 col = runway_urgent ? palette::negative
                                        : ImGui::GetColorU32(ImGuiCol_Text);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%s", runway_text.c_str());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", runway_tip.c_str());
    }

    if (show_spark)
    {
        ImGui::SameLine();
        ImGui::PlotLines("##balance_spark",
                         balance_history.data(),
                         static_cast<int>(balance_history.size()),
                         0, nullptr,
                         FLT_MAX, FLT_MAX,
                         {spark_width, ImGui::GetFrameHeight()});
    }

    ImGui::End();
}

} // namespace ui
