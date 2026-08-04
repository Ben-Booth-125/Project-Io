#include "tech_tree_panel.hpp"

#include "foldout_column.hpp"

#include <imgui.h>

#include <string>
#include <vector>

namespace ui {

namespace {

/// "invention" / "tier 2" / "capstone" — the kind column, with the tier folded in.
std::string kind_label(const tech_node& t)
{
    if (t.kind == "tier" && t.tier > 0)
        return "tier " + std::to_string(t.tier);
    return t.kind;
}

/// Join a string list with " + " for the prereqs / opens columns.
std::string join(const std::vector<std::string>& parts)
{
    std::string out;
    for (const std::string& p : parts)
    {
        if (!out.empty())
            out += " + ";
        out += p;
    }
    return out;
}

/// One quest's collapsing header + tech table.
void draw_quest(const tech_tree_registry& tree, const tech_quest& q)
{
    // Stable ### id so the label can change without ImGui losing the open state.
    const std::string header = q.name + "  [" + q.id + ", " + q.status + "]###" + q.id;
    if (!ImGui::CollapsingHeader(header.c_str()))
        return;

    if (!q.thesis.empty())
        ImGui::TextDisabled("%s", q.thesis.c_str());
    if (!q.opens.empty())
        ImGui::Text("Capstone opens: %s", join(q.opens).c_str());

    // Gather this quest's techs in authored order.
    std::vector<const tech_node*> nodes;
    for (const tech_node& t : tree.techs())
        if (t.quest == q.id)
            nodes.push_back(&t);

    if (nodes.empty())
    {
        ImGui::TextDisabled("(no techs enumerated)");
        return;
    }

    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
    const std::string table_id = "techs##" + q.id;
    if (ImGui::BeginTable(table_id.c_str(), 7, table_flags))
    {
        ImGui::TableSetupColumn("Id");
        ImGui::TableSetupColumn("Tech");
        ImGui::TableSetupColumn("Kind");
        ImGui::TableSetupColumn("Cost");
        ImGui::TableSetupColumn("Payoff");
        ImGui::TableSetupColumn("Condition");
        ImGui::TableSetupColumn("Prereqs");
        ImGui::TableHeadersRow();

        for (const tech_node* t : nodes)
        {
            ImGui::TableNextRow();
            // Capstones carry the quest goal; economic conditions are the marquee
            // nodes — both get a tint so the threading insight reads at a glance.
            const bool is_capstone = (t->kind == "capstone");
            const bool is_economic = (t->condition == "surplus" || t->condition == "market"
                                      || t->condition == "stockpile");
            if (is_capstone)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
                                       ImGui::GetColorU32(ImVec4{0.45f, 0.35f, 0.10f, 0.35f}));
            else if (is_economic)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
                                       ImGui::GetColorU32(ImVec4{0.15f, 0.35f, 0.45f, 0.35f}));

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(t->id.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(t->name.c_str());
            if (!t->unlocks.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s", t->unlocks.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(kind_label(*t).c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(t->cost.c_str());
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(t->payoff.c_str());
            ImGui::TableSetColumnIndex(5);
            ImGui::TextUnformatted(t->condition.c_str());
            ImGui::TableSetColumnIndex(6);
            ImGui::TextUnformatted(join(t->prereqs).c_str());
        }
        ImGui::EndTable();
    }
}

} // namespace

void draw_tech_tree_panel(const tech_tree_registry& tree, bool& open, int& view)
{
    if (!open)
        return;

    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(
        ImVec2{disp.x * 0.5f, disp.y * 0.5f}, ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(ImVec2{860.0f, 560.0f}, ImGuiCond_Appearing);
    if (ImGui::Begin("Tech Tree (mock)", &open, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::TextDisabled(
            "BL-087 design mock — read-only; the tech system is post-prototype. "
            "%d quests, %d techs. Hover a tech for its unlock.",
            static_cast<int>(tree.quests().size()), static_cast<int>(tree.techs().size()));
        ImGui::Separator();

        // One tab per era — each era carries its own tree; eras without authored
        // content hold a placeholder. Standing lines span eras, so they keep a tab
        // of their own rather than repeating under every era. nav_button strip, not
        // ImGui::BeginTabBar, whose header does not render in this build (see
        // foldout_column.hpp); re-clicking the active tab closes the window
        // (toggle rule).
        ui::nav_button("Era -1 Antiquity", 0, view, &open);
        ImGui::SameLine();
        ui::nav_button("Era 0 — Terrestrial", 1, view, &open);
        ImGui::SameLine();
        ui::nav_button("Era 1 — Early Space", 2, view, &open);
        ImGui::SameLine();
        ui::nav_button("Standing lines", 3, view, &open);
        ImGui::Separator();
        ImGui::Spacing();

        if (view == 0)
        {
            ImGui::TextDisabled("Placeholder — no in-engine tree yet.");
            ImGui::TextWrapped(
                "The ancient ladder (0 CE to campaign epoch): derived by the Era -1 "
                "sim from endowment and contact, never clicked. Its 58 techs, 8 "
                "vertex quests and 3 keystones are data in "
                "docs/research/ancient_tech_ladder.json; whether this surface "
                "renders them is BL-296's open question 4.");
        }
        else if (view == 1 || view == 2)
        {
            const int era = view - 1;
            bool any = false;
            for (const tech_quest& q : tree.quests())
                if (q.type == "gate" && q.era == era)
                {
                    draw_quest(tree, q);
                    any = true;
                }
            if (!any)
                ImGui::TextDisabled("Placeholder — no quests authored for this era yet.");
        }
        else
        {
            ImGui::TextDisabled("Deepen every Era; never gate an Era.");
            for (const tech_quest& q : tree.quests())
                if (q.type == "standing")
                    draw_quest(tree, q);
        }
    }
    ImGui::End();
}

} // namespace ui
