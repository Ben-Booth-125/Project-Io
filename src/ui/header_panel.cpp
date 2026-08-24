#include "header_panel.hpp"

#include "format.hpp"
#include "presentation.hpp"
#include "text_fit.hpp"

#include "world/economy_system.hpp" // BL-577: economy_report, for budgets[player].subsidies

#include <imgui.h>

#include <unordered_map>

namespace ui {

namespace {

/// The player corporation's running balance, or 0 if it has no component yet.
float player_balance(const world& w)
{
    const auto it = w.corporations.find(w.player_entity);
    return (it != w.corporations.end()) ? it->second.balance : 0.0f;
}

} // namespace

// Declared in header_panel.hpp; shared with the Budget ledger (BL-171) so both the
// header strip's STOCKPILE figure and the ledger's Cargo Value use one valuation.
float player_stockpile_value(const world& w)
{
    std::unordered_map<entity_id, const market_component*> by_body;
    by_body.reserve(w.markets.size());
    for (const auto& [mid, mc] : w.markets)
        by_body.emplace(mc.body, &mc);

    float value = 0.0f;
    for (const auto& [key, pool] : w.corp_body_pools)
    {
        if (key.first != w.player_entity)
            continue;
        const auto mit = by_body.find(key.second);
        if (mit == by_body.end())
            continue;
        const market_component& mc = *mit->second;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (pool.quantities[r] > 0.0f)
                value += pool.quantities[r] * mc.price[r];
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

    const float balance   = player_balance(w);
    const float valuation = player_stockpile_value(w);

    // Last-tick net change: the most recent balance step (newest minus previous).
    float net = 0.0f;
    if (balance_history.size() >= 2)
        net = balance_history.back() - balance_history[balance_history.size() - 2];

    // Guaranteed-fit strip (LAYOUT.md container vocabulary): the strip never wraps
    // to a second line. The three balance/stockpile/net figures are bounded-length
    // (fmt::credits/rate abbreviate to a handful of characters) and always drawn in
    // full; the decorative extras — the in-debt flag and the sparkline — are the
    // genuine last resort to drop first when the available width is too narrow to
    // hold everything, so a load-bearing number is never silently clipped.
    const bool         in_debt      = balance < 0.0f;
    const std::string  debt_text    = "[in debt - interest accruing]";
    const ImVec2       item_spacing = ImGui::GetStyle().ItemSpacing;
    const float        spark_width  = 96.0f;

    // BL-177 — the runway read: "how long until underwater". NET says the player
    // is losing money; it does not say how much cushion is left, which is the
    // half of the goal that lets them act before bankruptcy rather than discover
    // it. BL-171 removed the old runway deliberately, so this re-homes it rather
    // than reverting: it sits in the header beside the figures it is derived
    // from, and is load-bearing (measured into used_width, never dropped).
    //
    // Deliberately NOT an "infinite quarters" reading when profitable — a runway
    // only exists while burning. Growing/flat states say so in words.
    // Shown only while it MEANS something — burning cash, or already under. When
    // the balance is growing there is no runway to run out, and a "growing"
    // reading would be noise competing with NET for scarce header width. This is
    // also why it is not an "infinite quarters" reading: that number would be a
    // lie dressed as a figure.
    // BL-577: this tick's contract income (budget_result::subsidies) — the
    // SAME line the Balance ledger's new "Contract income" line reads
    // (balance_ledger.cpp). The runway math itself needs no separate term:
    // `net` is a plain balance delta, so a contract payment already moved it.
    // What the math CANNOT say on its own is that one-off "steady burn"
    // premise is wrong for a tick a contract paid out — a single lump sum
    // reading as a rosy (or merely less bad) quarter would mislead the
    // "assumes the burn holds steady" reader, so the tooltip names it.
    float subsidies = 0.0f;
    if (const auto bit = report.budgets.find(w.player_entity); bit != report.budgets.end())
        subsidies = bit->second.subsidies;

    std::string runway_text;
    std::string runway_tip;
    bool        runway_urgent = false;
    const bool  runway_meaningful = (balance < 0.0f) || (net < 0.0f);
    if (balance < 0.0f)
    {
        runway_text   = "in debt";
        runway_urgent = true;
        runway_tip    = "Already underwater - interest accrues each quarter.\n"
                        "Runway resumes once the balance is positive again.";
    }
    else if (net < 0.0f)
    {
        const int quarters = static_cast<int>(balance / -net);
        char      buf[32];
        std::snprintf(buf, sizeof(buf), "%dq", quarters);
        runway_text = buf;
        // Under two years of cushion is the point it needs acting on.
        runway_urgent = quarters < 8;
        char tip[256];
        if (subsidies > 0.0f)
            std::snprintf(tip, sizeof(tip),
                          "About %d quarters of cushion: balance divided by the current\n"
                          "quarterly loss. Assumes the burn holds steady - it moves as\n"
                          "prices, wages and upkeep change. Includes Cr %.0f of contract\n"
                          "income this quarter - a one-off payment, not a steady rate.",
                          quarters, static_cast<double>(subsidies));
        else
            std::snprintf(tip, sizeof(tip),
                          "About %d quarters of cushion: balance divided by the current\n"
                          "quarterly loss. Assumes the burn holds steady - it moves as\n"
                          "prices, wages and upkeep change.", quarters);
        runway_tip = tip;
    }

    float used_width = ImGui::CalcTextSize("BALANCE").x + item_spacing.x +
                       ImGui::CalcTextSize(fmt::credits(balance).c_str()).x + item_spacing.x +
                       ImGui::CalcTextSize("   |   STOCKPILE").x + item_spacing.x +
                       ImGui::CalcTextSize(fmt::credits(valuation).c_str()).x + item_spacing.x +
                       ImGui::CalcTextSize("   |   NET").x + item_spacing.x +
                       ImGui::CalcTextSize(fmt::rate(net, "qtr").c_str()).x;

    const float available   = ImGui::GetContentRegionAvail().x;
    bool        show_debt   = in_debt;
    bool        show_spark  = balance_history.size() >= 2;

    // BL-177 runway fit, resolved BEFORE the decorative drops because the runway
    // is load-bearing and they are not. Container 4 (LAYOUT.md) forbids silently
    // truncating a header figure, so this degrades in three honest steps rather
    // than letting the value run off the edge:
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

    if (show_debt && used_width + item_spacing.x + ImGui::CalcTextSize(debt_text.c_str()).x > available)
        show_debt = false; // drop the debt flag first — the red number already carries the signal
    else if (show_debt)
        used_width += item_spacing.x + ImGui::CalcTextSize(debt_text.c_str()).x;

    if (show_spark && used_width + item_spacing.x + spark_width > available)
        show_spark = false; // drop the sparkline next if still too tight

    // --- Balance (negatives flagged red, per the economy-panel convention) ---
    ImGui::TextDisabled("BALANCE");
    ImGui::SameLine();
    {
        // The figures draw through fit_text (BL-215): the drop ladder above runs
        // first, so elision is the genuine last resort container 4 permits.
        const ImU32 col = in_debt ? palette::negative : palette::positive;
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        fit_text(text_box::header_strip, "header.balance", fmt::credits(balance).c_str(),
                 ImGui::GetContentRegionAvail().x);
        ImGui::PopStyleColor();
    }

    // BL-073: explicit in-debt affordance — a negative balance is now self-
    // accelerating (interest accrues each quarter), so flag it plainly rather than
    // relying on the red number alone.
    if (show_debt)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative), "%s", debt_text.c_str());
    }
    else if (in_debt)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative), "[in debt]");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", debt_text.c_str());
    }

    // --- Estimated stockpile valuation ---
    ImGui::SameLine();
    ImGui::TextDisabled("   |   STOCKPILE");
    ImGui::SameLine();
    fit_text(text_box::header_strip, "header.stockpile", fmt::credits(valuation).c_str(),
             ImGui::GetContentRegionAvail().x);

    // --- Last-tick net + a sparkline of recent balances ---
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

    // --- BL-177: runway — how long the balance lasts at the current burn ---
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
