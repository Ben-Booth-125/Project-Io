#include "profile_panel.hpp"
#include "foldout_column.hpp"
#include "icons.hpp"
#include "presentation.hpp"
#include "text_fit.hpp"

#include <imgui.h>

#include <string>

namespace ui {

void draw_profile_panel(const world& w)
{
    // The identity tile caps the permanent left shell column (BL-122): it takes the
    // column's full width W, computed at runtime from the display so it stays legible
    // across resolutions. The balance bar and Selection element clear the same W.
    const float width = shell_column_width(ImGui::GetIO().DisplaySize.x);
    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize({width, profile_panel_height});

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

    // BL-1406 (PROFILE.md § Contents): the card names the player's NATION — the
    // player IS its nation (CONCEPT.md § Player identity). The emblem's shape is
    // keyed by the nation's id and its colour is the nation's identity colour, the
    // one pinned table the Nations lens and the border band read. The former
    // `Parent:` line retired with the corporation. A world with nobody seated
    // (spectate) falls back to the placeholder.
    const char*     nation_name  = "Unseated";
    const entity_id nation       = player_nation(w);
    const ImU32     emblem_col   = nation != null_entity
                                       ? palette::nation_colour(nation)
                                       : palette::corp_identity_colour(w.player_entity, w.player_entity);
    const int       emblem_shape = palette::corp_emblem_shape(nation != null_entity ? nation
                                                                                    : w.player_entity);
    if (const auto nat_it = w.nations.find(nation); nat_it != w.nations.end())
    {
        if (!nat_it->second.name.empty())
            nation_name = nat_it->second.name.c_str();
    }

    // Portrait slot: a dark rounded plate carrying the nation's geometric emblem
    // (shape + identity colour), with its name beside it.
    constexpr float portrait = 56.0f;
    const ImVec2    p0       = ImGui::GetCursorScreenPos();
    ImDrawList*     dl       = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, {p0.x + portrait, p0.y + portrait}, IM_COL32(28, 32, 42, 255), 4.0f);
    ui::icons::corp_emblem(dl, {p0.x + portrait * 0.5f, p0.y + portrait * 0.5f},
                           portrait * 0.34f, emblem_shape, emblem_col);
    dl->AddRect(p0, {p0.x + portrait, p0.y + portrait}, IM_COL32(110, 120, 140, 255), 4.0f);

    ImGui::SameLine(portrait + ImGui::GetStyle().ItemSpacing.x * 2.0f);
    ImGui::BeginGroup();
    // Ellipsize each line to the width remaining beside the portrait so long
    // generated names never spill past the fixed card edge; a truncated line
    // carries the full text as a hover tooltip (BL-091).
    const float avail = ImGui::GetContentRegionAvail().x;
    fit_text(text_box::header_strip, "profile.nation_name", nation_name, avail);
    // BL-145: focus readout hidden blanket (industrial_focus stays a data-model
    // field for world-gen/economy, just not surfaced in UI).
    ImGui::EndGroup();

    ImGui::End();
}

} // namespace ui
