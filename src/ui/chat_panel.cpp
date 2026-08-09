#include "chat_panel.hpp"

#include "presentation.hpp"
#include "text_fit.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

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
        return; // no vertical room in the bottom strip
    // Floor guard (BL-216). Below this the log child's `-input_h` height goes
    // negative, ImGui clamps it to a 4px sliver and the input row is pushed out of
    // the window entirely. Defensive only — the dock is `selection_band_height`
    // tall, which never resolves this low at a supported display size.
    if (height < 120.0f)
        return;

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

    // One control row: the COMMS label, the channel selector, the "+" group-create
    // button (BL-216). The label used to be a TextDisabled line of its own with a
    // Separator under it — two rows of chrome above a log whose scarce axis is now
    // VERTICAL, the dock being `selection_band_height` tall rather than the ~700px
    // right-column band it used to occupy.
    ImGui::TextDisabled("COMMS");
    ImGui::SameLine();

    // Channel SELECTOR, not a tab chain. BL-215 had already stopped the SameLine chain
    // clipping, by breaking the strip to a new line when the next tab would run past
    // the dock edge. That fixed the clipping and bought a different problem: rows. The
    // dock BL-227 landed is 260 px tall and ~246 px wide, so a wrapping strip spends
    // the vertical budget the message list needs, and spends more of it the more groups
    // the player creates.
    //
    // A combo holds an unbounded channel count in ONE fixed row, which is why it
    // supersedes the wrap rather than merely restating it. It is a cross-cutting
    // selector — it switches a target, it does not express an active state — so the
    // standing toggle rule explicitly exempts it.
    if (chat.active_channel < 0 || chat.active_channel >= static_cast<int>(chat.channels.size()))
        chat.active_channel = 0;
    const float plus_w = ImGui::GetFrameHeight();
    ImGui::SetNextItemWidth(
        std::max(60.0f, ImGui::GetContentRegionAvail().x - plus_w - ImGui::GetStyle().ItemSpacing.x));
    if (ImGui::BeginCombo("##chat_channel", chat.channels[chat.active_channel].name.c_str()))
    {
        for (int c = 0; c < static_cast<int>(chat.channels.size()); ++c)
        {
            const bool active = (c == chat.active_channel);
            ImGui::PushID(c);
            if (ImGui::Selectable(chat.channels[c].name.c_str(), active))
                chat.active_channel = c;
            if (active)
                ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("+", {plus_w, plus_w}))
    {
        // Seed the draft with every corp unticked; sorted ids for a stable order.
        chat.group_name[0] = '\0';
        chat.group_draft.clear();
        for (const auto& [id, corp] : w.corporations)
            chat.group_draft.emplace_back(id, false);
        std::sort(chat.group_draft.begin(), chat.group_draft.end());
        ImGui::OpenPopup("new_group");
    }
    // Size-capped (BL-216): the popup is bottom-anchored in the bottom-left corner,
    // so an uncapped member list grows UPWARDS without bound — one row per corp,
    // off the top of the screen at any real corp count.
    ImGui::SetNextWindowSizeConstraints({220.0f, 0.0f}, {320.0f, 260.0f});
    if (ImGui::BeginPopup("new_group"))
    {
        ImGui::TextDisabled("NEW GROUP");
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputTextWithHint("##group_name", "group name", chat.group_name,
                                 sizeof(chat.group_name));
        ImGui::Separator();
        if (ImGui::BeginChild("##grp_members", {0.0f, 160.0f}))
        {
            for (auto& [id, in] : chat.group_draft)
            {
                if (id == w.player_entity)
                    continue; // the creator is an implicit member
                ImGui::PushID(static_cast<int>(id));
                ImGui::Checkbox(corp_name(w, id), &in);
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
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
            // ONE wrapped paragraph, not the old two-line stanza (day+speaker on
            // its own TextWrapped line, then an Indent()ed body). Rows are the
            // scarce axis in the bottom strip and the stanza spent two of them on
            // every message plus an indent that bought nothing; folding the body
            // onto the speaker's line recovers roughly 40% of the log's rows.
            const char* const who   = speaker_name(w, m.from);
            const ImGuiStyle& style = ImGui::GetStyle();
            const float       gap   = style.ItemSpacing.x * 0.5f;

            char stamp[24];
            std::snprintf(stamp, sizeof(stamp), "d%d", m.day);

            // Keep the body on the speaker's line only while the prefix leaves it a
            // usable run. The dock is ~243px wide across 1280..1920, so a long
            // nation name plus the day stamp can eat most of the content width —
            // and TextWrapped with a non-positive wrap width degrades to one
            // character per line. Past the threshold the body takes the next row
            // instead, which is still a row cheaper than the old indented stanza.
            const float inner_w  = ImGui::GetContentRegionAvail().x;
            const float prefix_w = ImGui::CalcTextSize(stamp).x + gap
                                 + ImGui::CalcTextSize(who).x
                                 + ImGui::CalcTextSize(":").x + gap;

            ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
            ImGui::TextUnformatted(stamp);
            ImGui::PopStyleColor();
            ImGui::SameLine(0.0f, gap);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  speaker_colour(w, m.from, w.player_entity));
            ImGui::Text("%s:", who);
            ImGui::PopStyleColor();
            if (prefix_w < inner_w * 0.65f)
                ImGui::SameLine(0.0f, gap);
            ImGui::TextWrapped("%s", m.text.c_str());
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
