#include "construction_panel.hpp"

#include "ledger_chrome.hpp" // shared ledger-window size + spawn anchor
#include "presentation.hpp"
#include "ui_state.hpp"

#include "world/components.hpp"
#include "world/placement_rules.hpp"

#include <imgui.h>

namespace ui {

namespace {

// --- Build section -----------------------------------------------------------
// Arms placement mode by writing the shared construction_state. Nothing here
// mutates the world: in v0.0.5 placement is a preview seam (the Planetary canvas
// reads construction.active and draws a ghost marker); the functional construct
// loop lands in v0.0.6.
void draw_build_section(ui_state& state)
{
    if (!ImGui::CollapsingHeader("Build", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    // A Build button arms placement mode for its type. extraction_site keeps the
    // current target; the other types ignore it (placement_rules::can_place does too).
    auto arm = [&state](building_type type) {
        state.construction.active = true;
        state.construction.type   = type;
    };

    if (ImGui::Button("Extraction Site"))
        arm(building_type::extraction_site);
    ImGui::SameLine();
    if (ImGui::Button("Processing Facility"))
        arm(building_type::processing_facility);
    ImGui::SameLine();
    if (ImGui::Button("Port"))
        arm(building_type::port);

    // Target-resource selector — meaningful only when placing an extraction site.
    // Offers the prototype-extractable resources straight from the single source
    // of truth (placement_rules::k_extractable), writing construction.target.
    if (state.construction.type == building_type::extraction_site)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Extraction target");
        for (const resource_type r : placement_rules::k_extractable)
        {
            const bool selected = (state.construction.target == r);
            // A small toggle row; the armed target is highlighted via the selectable.
            if (ImGui::Selectable(resource_name(r), selected, 0, ImVec2(160.0f, 0.0f)))
                state.construction.target = r;
        }
    }

    ImGui::Spacing();

    // Current placement-mode status + a Cancel that disarms.
    if (state.construction.active)
    {
        if (state.construction.type == building_type::extraction_site)
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::pinned),
                "Placing: %s -> %s",
                building_type_name(state.construction.type),
                resource_name(state.construction.target));
        else
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::pinned),
                "Placing: %s", building_type_name(state.construction.type));

        if (ImGui::Button("Cancel"))
            state.construction.active = false;

        // Helper line: click a tile to place. v0.0.5 is a preview only — the click
        // is a non-mutating seam; no construction is actually committed yet.
        ImGui::TextDisabled("Click a tile on the surface to place it.");
        ImGui::TextDisabled("(v0.0.5 preview - placement does not build yet.)");
    }
    else
    {
        ImGui::TextDisabled("Choose a building above to begin placing.");
    }
}

// --- Selected-building section -----------------------------------------------
// Resolves the building whose tile is the current selection and shows its config
// read-only, followed by DISABLED stub controls. The stubs are wired to v0.0.6
// management logic later; here they only display the current values.
void draw_selected_section(const world& w, const recipe_registry& reg, const ui_state& state)
{
    if (!ImGui::CollapsingHeader("Selected building", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    // Find the building sitting on the selected tile (buildings key on their own
    // entity id, not their tile, so we scan for tile == selected_entity).
    const building_component* found = nullptr;
    for (const auto& [id, bld] : w.buildings)
    {
        if (bld.tile == state.selected_entity)
        {
            found = &bld;
            break;
        }
    }

    if (found == nullptr)
    {
        ImGui::TextDisabled("Select a building's tile to manage it.");
        return;
    }

    const building_component&  b   = *found;
    const building_economics&  eco = reg.economics(b.type);
    const recipe*              rcp = reg.get_recipe(b.recipe);

    // --- read-only detail ---
    ImGui::Text("Type: %s", building_type_name(b.type));

    // Target resource is meaningful only for extraction sites.
    if (b.type == building_type::extraction_site)
        ImGui::Text("Target: %s", resource_name(b.target_resource));
    else
        ImGui::TextDisabled("Target: -");

    // Recipe name; "-" when no recipe is assigned (no_recipe / unconfigured).
    if (rcp != nullptr && !rcp->name.empty())
        ImGui::Text("Recipe: %s", rcp->name.c_str());
    else
        ImGui::TextDisabled("Recipe: -");

    ImGui::Text("Workforce: %.0f%%", b.workforce_assigned * 100.0f);
    ImGui::Text("Build cost: %.1f", eco.build_cost);
    ImGui::Text("Maintenance: %.1f / tick", eco.maintenance);

    ImGui::Spacing();
    ImGui::SeparatorText("Management (v0.0.6)");

    // v0.0.5 DISABLED scaffolds: these controls display the current state but take
    // no input. They are wired to the v0.0.6 management logic (recipe authoring,
    // workforce allocation, sell orders) later — kept here so the surface exists.
    ImGui::BeginDisabled();

    // Recipe combo — shows the current recipe name; selection is inert for now.
    const char* preview = (rcp != nullptr && !rcp->name.empty()) ? rcp->name.c_str() : "-";
    if (ImGui::BeginCombo("Recipe##stub", preview))
        ImGui::EndCombo();

    // Workforce slider — shows the assigned fraction; drag is inert for now.
    float workforce = b.workforce_assigned;
    ImGui::SliderFloat("Workforce##stub", &workforce, 0.0f, 1.0f, "%.2f");

    ImGui::Button("Create sell order");

    ImGui::EndDisabled();
}

} // namespace

void draw_construction_panel(const world& w,
                             const recipe_registry& reg,
                             ui_state& state,
                             bool* p_open)
{
    if (p_open && !*p_open)
        return;

    // Shared ledger-window chrome (docs/ui/LAYOUT.md § Uniform ledger-window chrome).
    ImGui::SetNextWindowPos(ledger_window_spawn, ImGuiCond_Once);
    ImGui::SetNextWindowSize(ledger_window_size, ImGuiCond_Once);
    // Passing p_open gives the window a close button that clears the flag.
    ImGui::Begin("Construction", p_open);

    draw_build_section(state);
    ImGui::Spacing();
    draw_selected_section(w, reg, state);

    ImGui::End();
}

} // namespace ui
