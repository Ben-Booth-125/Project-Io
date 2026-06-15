#include "economy_panel.hpp"

#include "format.hpp"
#include "ledger_chrome.hpp" // shared ledger-window size + spawn anchor
#include "presentation.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace ui {

namespace {

/// Corp display name, or a synthetic "Corp #id" if the entry is missing.
std::string corp_label(const world& w, entity_id corp)
{
    const auto it = w.corporations.find(corp);
    if (it != w.corporations.end() && !it->second.name.empty())
        return it->second.name;
    return "Corp #" + std::to_string(corp);
}

/// Body display name, or a synthetic fallback.
std::string body_label(const world& w, entity_id body)
{
    const auto it = w.bodies.find(body);
    if (it != w.bodies.end())
        return it->second.name;
    return "Body #" + std::to_string(body);
}

/// A resource name rendered in its identity colour (the shared swatch convention).
void resource_text(resource_type r)
{
    const resource_presentation& rp = presentation_of(r);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(rp.colour), "%s", rp.name);
}

// --- corporation balances ----------------------------------------------------
void draw_balances(const world& w)
{
    if (!ImGui::CollapsingHeader("Corporation balances", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    std::vector<entity_id> corps;
    corps.reserve(w.corporations.size());
    for (const auto& [id, _] : w.corporations)
        corps.push_back(id);
    std::sort(corps.begin(), corps.end());

    if (ImGui::BeginTable("##balances", 3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Corporation");
        ImGui::TableSetupColumn("Focus");
        ImGui::TableSetupColumn("Balance");
        ImGui::TableHeadersRow();

        for (entity_id id : corps)
        {
            const corporation_component& cc = w.corporations.at(id);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            if (id == w.player_entity)
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::pinned), "%s", cc.name.c_str());
            else
                ImGui::TextUnformatted(cc.name.c_str());

            ImGui::TableSetColumnIndex(1);
            const char* focus =
                cc.focus == industrial_focus::extraction ? "Extraction" :
                cc.focus == industrial_focus::processing ? "Processing" : "Trade";
            ImGui::TextDisabled("%s", focus);

            ImGui::TableSetColumnIndex(2);
            // Negative balances are flagged red (palette::negative).
            const ImU32 col = (cc.balance < 0.0f) ? palette::negative : palette::positive;
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%.1f", cc.balance);
        }
        ImGui::EndTable();
    }
}

// --- building states ---------------------------------------------------------
void draw_buildings(const world& w, const recipe_registry& reg, const economy_report& report)
{
    if (!ImGui::CollapsingHeader("Buildings", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    if (report.buildings.empty())
    {
        ImGui::TextDisabled("No production has run yet (advance to an economy tick).");
        return;
    }

    if (ImGui::BeginTable("##buildings", 6,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Corporation");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Makes");
        ImGui::TableSetupColumn("Output");
        ImGui::TableSetupColumn("State");
        ImGui::TableSetupColumn("Limiting input");
        ImGui::TableHeadersRow();

        for (const building_report& br : report.buildings)
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(corp_label(w, br.corp).c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("%s", building_type_name(br.type));

            ImGui::TableSetColumnIndex(2);
            if (br.type == building_type::extraction_site)
            {
                resource_text(br.target_resource);
            }
            else if (br.type == building_type::processing_facility)
            {
                const recipe* rcp = reg.get_recipe(br.recipe);
                if (rcp && !rcp->name.empty())
                    ImGui::TextUnformatted(rcp->name.c_str());
                else
                    ImGui::TextDisabled("(no recipe)");
            }
            else
            {
                ImGui::TextDisabled("-");
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.1f", br.output_quantity);

            ImGui::TableSetColumnIndex(4);
            if (br.active)
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::positive), "active");
            else if (br.exhausted)
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative), "out of resources");
            else
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::neutral), "idle");

            ImGui::TableSetColumnIndex(5);
            if (br.type == building_type::processing_facility && br.has_limiting)
                resource_text(br.limiting_input);
            else
                ImGui::TextDisabled("-");
        }
        ImGui::EndTable();
    }
}

// --- (corp, body) pools ------------------------------------------------------
void draw_pools(const world& w)
{
    if (!ImGui::CollapsingHeader("Stockpile pools (corp x body)", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    if (w.corp_body_pools.empty())
    {
        ImGui::TextDisabled("No pools yet.");
        return;
    }

    for (const auto& [key, pool] : w.corp_body_pools)
    {
        const std::string title =
            corp_label(w, key.first) + "  @  " + body_label(w, key.second);

        // Skip wholly-empty pools to keep the panel legible.
        bool any = false;
        for (std::size_t r = 0; r < resource_count && !any; ++r)
            any = pool.quantities[r] > 0.0f;

        if (ImGui::TreeNodeEx(title.c_str(), any ? ImGuiTreeNodeFlags_DefaultOpen : 0))
        {
            if (!any)
            {
                ImGui::TextDisabled("(empty)");
            }
            else if (ImGui::BeginTable("##pool", 2,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Resource");
                ImGui::TableSetupColumn("Quantity");
                ImGui::TableHeadersRow();
                for (std::size_t r = 0; r < resource_count; ++r)
                {
                    if (pool.quantities[r] <= 0.0f)
                        continue;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    resource_text(static_cast<resource_type>(r));
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.1f", pool.quantities[r]);
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }
}

// --- workforce contention ----------------------------------------------------
// Surfaces the (corp, body) labour-pool contention from this tick's report. Only
// the throttled pools (scalar < 1.0) are listed — an uncontended economy shows the
// reassuring "all fully staffed" line (POPULATION.md § Workforce model, step 1).
void draw_workforce(const world& w, const economy_report& report)
{
    if (!ImGui::CollapsingHeader("Workforce (corp x body)", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    bool any = false;
    for (const auto& [key, scalar] : report.workforce_contention)
        if (scalar < 1.0f) { any = true; break; }

    if (!any)
    {
        ImGui::TextDisabled("All buildings fully staffed (no labour contention).");
        return;
    }

    if (ImGui::BeginTable("##workforce", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Corp @ body");
        ImGui::TableSetupColumn("Staffing");
        ImGui::TableHeadersRow();
        for (const auto& [key, scalar] : report.workforce_contention)
        {
            if (scalar >= 1.0f)
                continue;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const std::string title =
                corp_label(w, key.first) + "  @  " + body_label(w, key.second);
            ImGui::TextUnformatted(title.c_str());
            ImGui::TableSetColumnIndex(1);
            // Throttled labour reads as a warning colour and a percentage.
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                "%.0f%% (labour short)", scalar * 100.0f);
        }
        ImGui::EndTable();
    }
}

// --- markets -----------------------------------------------------------------
void draw_markets(const world& w)
{
    if (!ImGui::CollapsingHeader("Markets (supply / demand)", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    if (w.markets.empty())
    {
        ImGui::TextDisabled("No markets.");
        return;
    }

    std::vector<entity_id> mids;
    mids.reserve(w.markets.size());
    for (const auto& [id, _] : w.markets)
        mids.push_back(id);
    std::sort(mids.begin(), mids.end());

    for (entity_id mid : mids)
    {
        const market_component& mc = w.markets.at(mid);
        if (ImGui::TreeNodeEx(body_label(w, mc.body).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::BeginTable("##market", 4,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Resource");
                ImGui::TableSetupColumn("Supply");
                ImGui::TableSetupColumn("Demand");
                ImGui::TableSetupColumn("Price");
                ImGui::TableHeadersRow();
                for (std::size_t r = 0; r < resource_count; ++r)
                {
                    // Show only resources that trade here (a non-zero base price).
                    if (mc.base_price[r] <= 0.0f)
                        continue;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    resource_text(static_cast<resource_type>(r));
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%.1f", mc.supply[r]);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%.1f", mc.demand[r]);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::Text("%.2f", mc.price[r]);
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }
}

} // namespace

void draw_economy_panel(const world& w,
                        const recipe_registry& reg,
                        const economy_report& report,
                        bool* p_open)
{
    if (p_open && !*p_open)
        return;

    // Shared ledger-window chrome (docs/ui/LAYOUT.md § Uniform ledger-window chrome).
    ImGui::SetNextWindowPos(ledger_window_spawn, ImGuiCond_Once);
    ImGui::SetNextWindowSize(ledger_window_size, ImGuiCond_Once);
    ImGui::Begin("Economy", p_open);

    draw_balances(w);
    draw_buildings(w, reg, report);
    draw_workforce(w, report);
    draw_pools(w);
    draw_markets(w);

    ImGui::End();
}

} // namespace ui
