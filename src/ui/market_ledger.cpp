#include "market_ledger.hpp"

#include "ledger_chrome.hpp"
#include "presentation.hpp"

#include <imgui.h>

#include <algorithm>
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

} // namespace

void draw_market_ledger(const world& w, const ui_state& /*s*/, bool& open)
{
    if (!open)
        return;

    ImGui::SetNextWindowPos(ledger_window_spawn, ImGuiCond_Once);
    ImGui::SetNextWindowSize(ledger_window_size, ImGuiCond_Once);

    if (!ImGui::Begin("Market Ledger", &open))
    {
        ImGui::End();
        return;
    }

    if (w.markets.empty())
    {
        ImGui::TextDisabled("No markets.");
        ImGui::End();
        return;
    }

    // Build a sorted list of market entity IDs for deterministic iteration.
    std::vector<entity_id> mids;
    mids.reserve(w.markets.size());
    for (const auto& [id, _] : w.markets)
        mids.push_back(id);
    std::sort(mids.begin(), mids.end());

    // Body selector — default to the market whose body matches w.home_body.
    static entity_id selected_market = null_entity;
    if (selected_market == null_entity || w.markets.find(selected_market) == w.markets.end())
    {
        // Prefer the home-body market; fall back to the first sorted entry.
        selected_market = mids.front();
        for (entity_id mid : mids)
        {
            if (w.markets.at(mid).body == w.home_body)
            {
                selected_market = mid;
                break;
            }
        }
    }

    // Combo to pick which body's market to display.
    const std::string preview = body_label(w, w.markets.at(selected_market).body);
    if (ImGui::BeginCombo("Body", preview.c_str()))
    {
        for (entity_id mid : mids)
        {
            const bool is_selected = (mid == selected_market);
            const std::string label = body_label(w, w.markets.at(mid).body);
            if (ImGui::Selectable(label.c_str(), is_selected))
                selected_market = mid;
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    const market_component& mc = w.markets.at(selected_market);

    // Resource table: Resource | Supply | Demand | Price | Net balance
    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp;

    float total_cleared = 0.0f;

    if (ImGui::BeginTable("##market_ledger", 5, table_flags))
    {
        ImGui::TableSetupColumn("Resource");
        ImGui::TableSetupColumn("Supply");
        ImGui::TableSetupColumn("Demand");
        ImGui::TableSetupColumn("Price");
        ImGui::TableSetupColumn("Net");
        ImGui::TableHeadersRow();

        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float supply = mc.supply[r];
            const float demand = mc.demand[r];

            // Show only resources that have at least one offer or bid.
            if (supply <= 0.0f && demand <= 0.0f)
                continue;

            const float net = supply - demand;
            total_cleared += std::min(supply, demand);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            {
                const resource_presentation& rp =
                    presentation_of(static_cast<resource_type>(r));
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(rp.colour),
                                   "%s", rp.name);
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.1f", supply);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.1f", demand);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f", mc.price[r]);

            ImGui::TableSetColumnIndex(4);
            {
                ImU32 col;
                if (net > 0.0f)       col = palette::positive;
                else if (net < 0.0f)  col = palette::negative;
                else                  col = palette::neutral;
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col),
                                   "%+.1f", net);
            }
        }

        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::Text("Turnover this tick: %.1f", total_cleared);

    ImGui::End();
}

} // namespace ui
