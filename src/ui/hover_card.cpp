#include "hover_card.hpp"

#include <imgui_internal.h>

#include <algorithm>

namespace ui {

void draw_hover_card(ImVec2 anchor,
                     const std::function<void()>& content,
                     ImVec2* out_min,
                     ImVec2* out_max)
{
    constexpr float kMaxWidth  = 200.0f;
    constexpr float kPad       = 6.0f;
    constexpr float kRounding  = 4.0f;
    constexpr float kYOffset   = 18.0f; // above the anchor point

    // Pinned to the anchor the card was summoned at, NOT the live cursor
    // (BL-228): a card that slides with the pointer cannot be read to the end of
    // a line, and offers nowhere to move the pointer TO in order to dismiss it.
    const ImVec2 pos = { anchor.x + 4.0f, anchor.y - kYOffset };

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, {0.0f, 1.0f});
    ImGui::SetNextWindowSize({ kMaxWidth, 0.0f }); // 0 height = auto
    ImGui::SetNextWindowBgAlpha(0.82f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
                           | ImGuiWindowFlags_NoResize
                           | ImGuiWindowFlags_NoScrollbar
                           | ImGuiWindowFlags_NoInputs
                           | ImGuiWindowFlags_NoMove
                           | ImGuiWindowFlags_NoSavedSettings
                           | ImGuiWindowFlags_NoFocusOnAppearing
                           | ImGuiWindowFlags_NoNav
                           | ImGuiWindowFlags_AlwaysAutoResize
                           | ImGuiWindowFlags_Tooltip; // keeps it above other windows

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  { kPad, kPad });

    if (ImGui::Begin("##hover_card", nullptr, flags))
        content();

    // Report the rect the card actually occupied so the caller can hit-test the
    // pointer against it next frame and dismiss on exit. Read while the window is
    // still current — the auto-resized height is only known here, after the
    // content has been laid out.
    const ImVec2 wmin = ImGui::GetWindowPos();
    const ImVec2 wsz  = ImGui::GetWindowSize();
    if (out_min) *out_min = wmin;
    if (out_max) *out_max = { wmin.x + wsz.x, wmin.y + wsz.y };

    ImGui::End();

    ImGui::PopStyleVar(2);
}

// ---------------------------------------------------------------------------
// The freezable tooltip (TOOLTIP.md § The freezable tooltip)
// ---------------------------------------------------------------------------

namespace {

/// The one freezable tooltip showing: whose it is, how long its item has been hovered,
/// and, once frozen, where it stands and the rect it took last frame.
struct freeze_state
{
    ImGuiID owner      = 0;
    float   dwell      = 0.0f;
    bool    frozen     = false;
    ImVec2  pos        = {0.0f, 0.0f};
    int     seen_frame = -1; ///< Last frame its owner called: a row that stopped drawing drops it.
};

freeze_state g_freeze;

/// The freeze's progress wheel at the tooltip's top-right corner: a faint ring the dwell
/// fills clockwise from twelve o'clock, @p frac of the way; full once frozen. Drawn over the
/// current window, so call it inside the tooltip, after its content.
void draw_freeze_wheel(float frac)
{
    ImDrawList*  dl  = ImGui::GetWindowDrawList();
    const ImVec2 wp  = ImGui::GetWindowPos();
    const ImVec2 ws  = ImGui::GetWindowSize();
    const float  r   = ImGui::GetFontSize() * 0.32f;
    const float  pad = ImGui::GetStyle().WindowPadding.x * 0.5f;
    const ImVec2 c   = {wp.x + ws.x - pad - r, wp.y + pad + r};
    const float  th  = std::max(1.5f, r * 0.3f);
    dl->AddCircle(c, r, ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.45f), 24, th);
    frac = std::clamp(frac, 0.0f, 1.0f);
    if (frac <= 0.0f)
        return;
    constexpr float kTop = -1.5707963f; // twelve o'clock
    dl->PathArcTo(c, r, kTop, kTop + frac * 6.2831853f, 24);
    dl->PathStroke(ImGui::GetColorU32(ImGuiCol_Text), ImDrawFlags_None, th);
}

} // namespace

bool freezable_tooltip(const char* id, float dt, const std::function<void()>& content)
{
    const ImGuiID me      = ImGui::GetID(id);
    const bool    hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    const int     frame   = ImGui::GetFrameCount();
    if (g_freeze.owner != 0 && g_freeze.seen_frame < frame - 1)
        g_freeze = {}; // its item is no longer drawn (the ledger closed, the row went)

    if (g_freeze.frozen)
    {
        if (g_freeze.owner != me)
            return false;
        g_freeze.seen_frame = frame;
        // A plain window, so its items take the pointer and carry their own tooltips; held
        // at the front, where the tooltip stood when it froze.
        ImGui::SetNextWindowPos(g_freeze.pos, ImGuiCond_Always);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_PopupBg));
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                                     | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
                                     | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
                                     | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;
        ImVec2 wmin{}, wmax{};
        if (ImGui::Begin("##freezable_tooltip", nullptr, flags))
        {
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
            content();
            draw_freeze_wheel(1.0f);
        }
        wmin = ImGui::GetWindowPos();
        const ImVec2 wsz = ImGui::GetWindowSize();
        wmax = {wmin.x + wsz.x, wmin.y + wsz.y};
        ImGui::End();
        ImGui::PopStyleColor();
        const ImVec2 mp  = ImGui::GetMousePos();
        const float  pad = kHoverCardExitPadPx;
        const bool inside = mp.x >= wmin.x - pad && mp.x <= wmax.x + pad
                         && mp.y >= wmin.y - pad && mp.y <= wmax.y + pad;
        if (!inside && !hovered)
            g_freeze = {};
        return g_freeze.frozen;
    }

    if (!hovered)
    {
        if (g_freeze.owner == me)
            g_freeze = {};
        return false;
    }
    if (g_freeze.owner != me)
    {
        g_freeze       = {};
        g_freeze.owner = me;
    }
    g_freeze.seen_frame = frame;
    g_freeze.dwell += dt;
    ImGui::BeginTooltip();
    content();
    draw_freeze_wheel(g_freeze.dwell / kFreezableTooltipFreezeSec);
    const ImVec2 at = ImGui::GetWindowPos();
    ImGui::EndTooltip();
    if (g_freeze.dwell >= kFreezableTooltipFreezeSec - kHoverThresholdEpsilon)
    {
        g_freeze.frozen = true;
        g_freeze.pos    = at;
    }
    return g_freeze.frozen;
}

bool freezable_tooltip_frozen(unsigned* owner)
{
    if (owner != nullptr)
        *owner = g_freeze.frozen ? g_freeze.owner : 0u;
    return g_freeze.frozen;
}

} // namespace ui
