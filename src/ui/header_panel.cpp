#include "header_panel.hpp"

#include "format.hpp"
#include "presentation.hpp"

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

    float used_width = ImGui::CalcTextSize("BALANCE").x + item_spacing.x +
                       ImGui::CalcTextSize(fmt::credits(balance).c_str()).x + item_spacing.x +
                       ImGui::CalcTextSize("   |   STOCKPILE").x + item_spacing.x +
                       ImGui::CalcTextSize(fmt::credits(valuation).c_str()).x + item_spacing.x +
                       ImGui::CalcTextSize("   |   NET").x + item_spacing.x +
                       ImGui::CalcTextSize(fmt::rate(net, "qtr").c_str()).x;

    const float available   = ImGui::GetContentRegionAvail().x;
    bool        show_debt   = in_debt;
    bool        show_spark  = balance_history.size() >= 2;

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
        const ImU32 col = in_debt ? palette::negative : palette::positive;
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%s", fmt::credits(balance).c_str());
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
    ImGui::Text("%s", fmt::credits(valuation).c_str());

    // --- Last-tick net + a sparkline of recent balances ---
    ImGui::SameLine();
    ImGui::TextDisabled("   |   NET");
    ImGui::SameLine();
    {
        const ImU32 col = value_colour(fmt::sign_of(net));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%s", fmt::rate(net, "qtr").c_str());
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
