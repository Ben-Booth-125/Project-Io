#include "profile_panel.hpp"
#include "presentation.hpp"

#include <imgui.h>

#include <cstdint>
#include <string>

namespace ui {

namespace {

/// Truncate @p text with a trailing ellipsis so it fits within @p max_w pixels at
/// the current font (BL-091: the fixed-width identity card clipped long corp / nation
/// names). Returns the text unchanged when it already fits. ASCII-oriented (the
/// prototype's generated names), so it trims by byte — adequate for the name banks.
std::string fit_ellipsis(const char* text, float max_w)
{
    if (max_w <= 0.0f || ImGui::CalcTextSize(text).x <= max_w)
        return text;
    const float dots = ImGui::CalcTextSize("...").x;
    std::string s = text;
    while (!s.empty() && ImGui::CalcTextSize(s.c_str()).x + dots > max_w)
        s.pop_back();
    return s + "...";
}

/// Draw @p text ellipsized to @p max_w (BL-091). When the line was truncated the
/// full string is exposed as a hover tooltip, so long generated names stay
/// readable without disturbing the fixed card geometry the header/nav key off.
void text_ellipsized(const char* text, float max_w, bool disabled)
{
    const std::string fitted = fit_ellipsis(text, max_w);
    if (disabled)
        ImGui::TextDisabled("%s", fitted.c_str());
    else
        ImGui::TextUnformatted(fitted.c_str());
    if (fitted != text)
        ImGui::SetItemTooltip("%s", text);
}

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

/// Number of distinct geometric corp emblems. A corporation's emblem is
/// (shape, identity colour); the shape is chosen deterministically from the
/// corp entity id so it is stable for a campaign and distinct between corps.
/// Prototype scope: rendered in the player identity card only. Promotion to a
/// shared ui::icons glyph family + map/selection markers is a backlog item.
constexpr int emblem_shape_count = 6;

/// Draw corporation emblem @p shape (0..emblem_shape_count-1) centred at @p c
/// with circumradius @p r, filled in @p col. Simple, legible primitives so the
/// emblem reads at portrait size and later at marker size.
void draw_corp_emblem(ImDrawList* dl, ImVec2 c, float r, int shape, ImU32 col)
{
    switch (shape % emblem_shape_count)
    {
        case 0: // circle
            dl->AddCircleFilled(c, r, col, 24);
            break;
        case 1: // square
            dl->AddRectFilled({c.x - r * 0.85f, c.y - r * 0.85f},
                              {c.x + r * 0.85f, c.y + r * 0.85f}, col);
            break;
        case 2: // upward triangle
            dl->AddTriangleFilled({c.x, c.y - r},
                                  {c.x + r * 0.92f, c.y + r * 0.7f},
                                  {c.x - r * 0.92f, c.y + r * 0.7f}, col);
            break;
        case 3: // diamond
            dl->AddQuadFilled({c.x, c.y - r}, {c.x + r, c.y},
                              {c.x, c.y + r}, {c.x - r, c.y}, col);
            break;
        case 4: // hexagon
            dl->AddNgonFilled(c, r, col, 6);
            break;
        default: // 5 — pentagon
            dl->AddNgonFilled(c, r, col, 5);
            break;
    }
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

    // Resolve the player corporation and its parent (registering) nation. Both are
    // set by corporation generation; fall back to placeholders only if the lookup
    // fails (e.g. a world with no player corp).
    const char* corp_name   = "Unnamed Corp";
    const char* parent_name = "—";
    const char* focus_name  = "—";
    // Emblem: the player's identity colour (corp slot 0) and a shape chosen
    // deterministically from the corp id so it is stable and corp-distinct.
    ImU32 emblem_col   = palette::corp_colour(0);
    int   emblem_shape = 0;
    const auto  corp_it = w.corporations.find(w.player_entity);
    if (corp_it != w.corporations.end())
    {
        const corporation_component& corp = corp_it->second;
        if (!corp.name.empty())
            corp_name = corp.name.c_str();
        focus_name = focus_label(corp.focus);
        const auto nat_it = w.nations.find(corp.home_nation);
        if (nat_it != w.nations.end() && !nat_it->second.name.empty())
            parent_name = nat_it->second.name.c_str();
        emblem_shape = static_cast<int>(
            (static_cast<uint32_t>(w.player_entity) * 2654435761u) % emblem_shape_count);
    }

    // Portrait slot: a dark rounded plate carrying the corporation's geometric
    // emblem (shape + identity colour), with the name / parent / focus beside it.
    constexpr float portrait = 56.0f;
    const ImVec2    p0       = ImGui::GetCursorScreenPos();
    ImDrawList*     dl       = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, {p0.x + portrait, p0.y + portrait}, IM_COL32(28, 32, 42, 255), 4.0f);
    draw_corp_emblem(dl, {p0.x + portrait * 0.5f, p0.y + portrait * 0.5f},
                     portrait * 0.34f, emblem_shape, emblem_col);
    dl->AddRect(p0, {p0.x + portrait, p0.y + portrait}, IM_COL32(110, 120, 140, 255), 4.0f);

    ImGui::SameLine(portrait + ImGui::GetStyle().ItemSpacing.x * 2.0f);
    ImGui::BeginGroup();
    // Ellipsize each line to the width remaining beside the portrait so long
    // generated names never spill past the fixed card edge; a truncated line
    // carries the full text as a hover tooltip (BL-091).
    const float avail = ImGui::GetContentRegionAvail().x;
    text_ellipsized(corp_name, avail, false);
    text_ellipsized((std::string("Parent: ") + parent_name).c_str(), avail, true);
    text_ellipsized((std::string("Focus: ") + focus_name).c_str(), avail, true);
    ImGui::EndGroup();

    ImGui::End();
}

} // namespace ui
