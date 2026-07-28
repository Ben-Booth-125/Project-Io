#include "chat_panel.hpp"

#include "presentation.hpp"

#include <imgui.h>

#include <algorithm>

namespace ui {

namespace {

/// Retained log cap; oldest lines drop first. Session-local (BL-205 slice 1).
constexpr std::size_t k_message_cap = 300;

/// Corp display name, tolerant of a stale id.
const char* corp_name(const world& w, entity_id corp)
{
    const auto it = w.corporations.find(corp);
    return it != w.corporations.end() ? it->second.name.c_str() : "(unknown)";
}

/// Display name for a chat_message author of EITHER kind (BL-212: Public is
/// nation-authored, Counsel channels stay corp-authored) — tries the nation
/// map first since only Public messages use it.
const char* speaker_name(const world& w, entity_id from)
{
    if (const auto nit = w.nations.find(from); nit != w.nations.end())
        return nit->second.name.c_str();
    return corp_name(w, from);
}

/// Identity colour for a chat_message author of either kind, mirroring
/// speaker_name's lookup order.
ImU32 speaker_colour(const world& w, entity_id from, entity_id player)
{
    if (w.nations.find(from) != w.nations.end())
        return palette::nation_colour(from);
    return palette::corp_identity_colour(from, player);
}

} // namespace

void chat_post(chat_state& chat, int day, entity_id from, int channel, std::string text)
{
    if (channel < 0 || channel >= static_cast<int>(chat.channels.size()))
        channel = 0;
    chat.messages.push_back({day, from, channel, std::move(text)});
    if (chat.messages.size() > k_message_cap)
        chat.messages.erase(chat.messages.begin(),
                            chat.messages.begin() + (chat.messages.size() - k_message_cap));
}

void draw_chat_panel(const world& w, chat_state& chat, int day,
                     float x, float y, float width, float height)
{
    if (height <= 0.0f)
        return; // no vertical room between the time column and the minimap

    ImGui::SetNextWindowPos({x, y});
    ImGui::SetNextWindowSize({width, height});

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("##chat_panel", nullptr, flags);

    ImGui::TextDisabled("COMMS");
    ImGui::Separator();

    // Channel tabs: Public + created groups, then the "+" group-create popup.
    for (int c = 0; c < static_cast<int>(chat.channels.size()); ++c)
    {
        if (c > 0)
            ImGui::SameLine();
        const bool active = (c == chat.active_channel);
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::SmallButton(chat.channels[c].name.c_str()))
            chat.active_channel = c;
        if (active)
            ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("+"))
    {
        // Seed the draft with every corp unticked; sorted ids for a stable order.
        chat.group_name[0] = '\0';
        chat.group_draft.clear();
        for (const auto& [id, corp] : w.corporations)
            chat.group_draft.emplace_back(id, false);
        std::sort(chat.group_draft.begin(), chat.group_draft.end());
        ImGui::OpenPopup("new_group");
    }
    if (ImGui::BeginPopup("new_group"))
    {
        ImGui::TextDisabled("NEW GROUP");
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputTextWithHint("##group_name", "group name", chat.group_name,
                                 sizeof(chat.group_name));
        ImGui::Separator();
        for (auto& [id, in] : chat.group_draft)
        {
            if (id == w.player_entity)
                continue; // the creator is an implicit member
            ImGui::PushID(static_cast<int>(id));
            ImGui::Checkbox(corp_name(w, id), &in);
            ImGui::PopID();
        }
        ImGui::Separator();
        const bool any = std::any_of(chat.group_draft.begin(), chat.group_draft.end(),
                                     [](const auto& p) { return p.second; });
        ImGui::BeginDisabled(chat.group_name[0] == '\0' || !any);
        if (ImGui::Button("Create"))
        {
            chat_channel ch;
            ch.name = chat.group_name;
            ch.members.push_back(w.player_entity);
            for (const auto& [id, in] : chat.group_draft)
                if (in)
                    ch.members.push_back(id);
            chat.channels.push_back(std::move(ch));
            chat.active_channel = static_cast<int>(chat.channels.size()) - 1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }

    // Message log for the active channel; keep pinned to the newest line while
    // the user hasn't scrolled back.
    const float input_h = ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("##chat_log", {0.0f, -input_h}, ImGuiChildFlags_None,
                          ImGuiWindowFlags_None))
    {
        for (const chat_message& m : chat.messages)
        {
            if (m.channel != chat.active_channel)
                continue;
            if (m.from == null_entity)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
                ImGui::TextWrapped("d%d  %s", m.day, m.text.c_str());
                ImGui::PopStyleColor();
                continue;
            }
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  speaker_colour(w, m.from, w.player_entity));
            ImGui::TextWrapped("d%d  %s:", m.day, speaker_name(w, m.from));
            ImGui::PopStyleColor();
            ImGui::Indent();
            ImGui::TextWrapped("%s", m.text.c_str());
            ImGui::Unindent();
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f)
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    // Player input: posts to the active channel. No mechanical effect yet — the
    // hook the AI C-route consumes (AI_OPPONENT.md § 7).
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputTextWithHint("##chat_input", "message...", chat.input,
                                 sizeof(chat.input),
                                 ImGuiInputTextFlags_EnterReturnsTrue))
    {
        if (chat.input[0] != '\0')
        {
            chat_post(chat, day, w.player_entity, chat.active_channel, chat.input);
            chat.input[0] = '\0';
        }
        ImGui::SetKeyboardFocusHere(-1); // keep the box focused for a follow-up line
    }

    ImGui::End();
}

} // namespace ui
