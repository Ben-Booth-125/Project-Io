#include "profile_panel.hpp"

#include <imgui.h>

namespace ui {

namespace {
/// Human-readable label for a corporation's industrial focus.
const char* focus_label(industrial_focus f)
{
    switch (f)
    {
        case industrial_focus::extraction: return "Extraction";
        case industrial_focus::processing: return "Processing";
        case industrial_focus::trade:      return "Trade";
    }
    return "—";
}
} // namespace

void draw_profile_panel(const world& w)
{
    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize({profile_panel_width, profile_panel_height});

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoScrollbar         |
        ImGuiWindowFlags_NoNav               |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("##profile_panel", nullptr, flags);

    // Portrait placeholder: a filled square to the left, with the corporation
    // name and basic standing stacked beside it. A real emblem/picture replaces
    // the square later.
    constexpr float portrait = 56.0f;
    const ImVec2    p0       = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        p0, {p0.x + portrait, p0.y + portrait}, IM_COL32(60, 70, 90, 255));
    ImGui::GetWindowDrawList()->AddRect(
        p0, {p0.x + portrait, p0.y + portrait}, IM_COL32(110, 120, 140, 255));

    // Resolve the player corporation and its parent (registering) nation. Both are
    // set by corporation generation; fall back to placeholders only if the lookup
    // fails (e.g. a world with no player corp).
    const char* corp_name   = "Unnamed Corp";
    const char* parent_name = "—";
    const char* focus_name  = "—";
    const auto  corp_it     = w.corporations.find(w.player_entity);
    if (corp_it != w.corporations.end())
    {
        const corporation_component& corp = corp_it->second;
        if (!corp.name.empty())
            corp_name = corp.name.c_str();
        focus_name = focus_label(corp.focus);
        const auto nat_it = w.nations.find(corp.home_nation);
        if (nat_it != w.nations.end() && !nat_it->second.name.empty())
            parent_name = nat_it->second.name.c_str();
    }

    ImGui::SameLine(portrait + ImGui::GetStyle().ItemSpacing.x * 2.0f);
    ImGui::BeginGroup();
    ImGui::TextUnformatted(corp_name);
    ImGui::TextDisabled("Parent: %s", parent_name);
    ImGui::TextDisabled("Focus: %s", focus_name);
    ImGui::EndGroup();

    ImGui::End();
}

} // namespace ui
