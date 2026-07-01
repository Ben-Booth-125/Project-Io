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

/// Estimated liquid value of everything the player holds: each player `(corp, body)`
/// pool's quantities priced at that body's current market price. Resources on a body
/// with no market (or no price) contribute nothing.
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

} // namespace

void draw_header_panel(const world& w,
                       const std::vector<float>& balance_history,
                       float left,
                       float right)
{
    constexpr float margin = 8.0f;
    const float     width  = right - left;
    if (width <= 0.0f)
        return; // window too narrow to show the strip

    ImGui::SetNextWindowPos({left, margin});
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

    const float balance   = player_balance(w);
    const float valuation = player_stockpile_value(w);

    // Last-tick net change: the most recent balance step (newest minus previous).
    float net = 0.0f;
    if (balance_history.size() >= 2)
        net = balance_history.back() - balance_history[balance_history.size() - 2];

    // --- Balance (negatives flagged red, per the economy-panel convention) ---
    ImGui::TextDisabled("BALANCE");
    ImGui::SameLine();
    {
        const ImU32 col = (balance < 0.0f) ? palette::negative : palette::positive;
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%s", fmt::credits(balance).c_str());
    }

    // BL-073: explicit in-debt affordance — a negative balance is now self-
    // accelerating (interest accrues each quarter), so flag it plainly rather than
    // relying on the red number alone.
    if (balance < 0.0f)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                           "[in debt - interest accruing]");
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
    if (balance_history.size() >= 2)
    {
        ImGui::SameLine();
        ImGui::PlotLines("##balance_spark",
                         balance_history.data(),
                         static_cast<int>(balance_history.size()),
                         0, nullptr,
                         FLT_MAX, FLT_MAX,
                         {96.0f, header_panel_height - 2.0f * margin});
    }

    ImGui::End();
}

} // namespace ui
