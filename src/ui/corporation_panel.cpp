#include "corporation_panel.hpp"

#include "foldout_column.hpp" // shell fold-out column host (BL-122)
#include "presentation.hpp"

#include "world/corp_command.hpp" // BL-449: the stance presses issue corp_command records
#include "world/stance.hpp"       // BL-448: is_hostile / are_friends / has_pending_friendship_offer
#include "world/survey_system.hpp" // survey_tile_visible — the BL-068 gate corp_is_discovered reads

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace ui {

namespace {

// BL-449: gates the stance column on ordinary BL-068 competitor visibility (NR-350 —
// a hostile declaration stays SILENT, discovered on contact rather than announced).
// There is no corp-level "discovered" flag in the data model; BL-068's single branch
// point is per-BUILDING (rival buildings are visible on-canvas, subject to the
// geographic/survey fog). A corp reads as discovered here iff the player can already
// see at least one of its buildings — i.e. it owns a building on a tile whose survey
// region is currently revealed. The player's own corp is always "discovered".
bool corp_is_discovered(const world& w, entity_id corp)
{
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return false;
    if (cit->second.is_player)
        return true;

    for (entity_id building : cit->second.assets)
    {
        const auto bit = w.buildings.find(building);
        if (bit == w.buildings.end())
            continue;
        const auto tit = w.tiles.find(bit->second.tile);
        if (tit == w.tiles.end())
            continue;
        const tile_component& t = tit->second;
        const auto bodyit = w.bodies.find(t.body);
        if (bodyit == w.bodies.end())
            continue;
        const body_component& bc = bodyit->second;
        if (survey_tile_visible(bc.survey, bc.grid_width, bc.grid_height, t.grid_x, t.grid_y))
            return true;
    }
    return false;
}

/// Short label for the player's stance toward `rival`, read via the directed
/// `is_hostile` and symmetric `are_friends` queries (never collapsed into one
/// accessor — stance.hpp's own invariant).
const char* stance_label(const world& w, entity_id player, entity_id rival)
{
    if (is_hostile(w, player, rival))
        return "Hostile";
    if (are_friends(w, player, rival))
        return "Friend";
    return "Neutral";
}

} // namespace

void draw_corporation_panel(const world& w, const std::vector<corp_standing>& standings,
                             ui_state& s, bool& open)
{
    if (!open)
        return;

    // Re-hosted into the shell fold-out column (BL-122); closed via the nav rail.
    if (!ui::foldout_begin("Corporations"))
    {
        ui::foldout_end();
        return;
    }

    // BL-111: SizingStretchProp collapsed all six columns to a single leading glyph in
    // the narrow shell column ('C F H C B S' headers, 'F T C 9 1 A' rows). Apply the
    // BL-081 pattern — stretch the identity column, fixed widths for the numerics — and
    // drop the two lowest-value columns so the four that matter fit legibly: Home Nation
    // is reachable by selecting the corp (Selection panel), and Status was only
    // Player/Active, already carried by the row tint + name colour. The reduced table is
    // the single question "how is each corporation doing?" (settles BL-121).
    //
    // BL-262 first slice: Balance was an exact number for every corp, including rivals —
    // a privacy violation against BL-068's competitor-visibility rule (rival internals are
    // private). Replaced with a three-axis standing profile (Reach / Capital / Share); the
    // player's row still shows exact figures, every rival row shows a band label only, no
    // numbers. No totals/aggregate row is added — BL-262's hard rule.
    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

    const entity_id player = w.player_entity;

    if (ImGui::BeginTable("##corps", 5, table_flags))
    {
        ImGui::TableSetupColumn("Corporation", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Reach",   ImGuiTableColumnFlags_WidthFixed, 62.0f);
        ImGui::TableSetupColumn("Capital", ImGuiTableColumnFlags_WidthFixed, 62.0f);
        ImGui::TableSetupColumn("Share",   ImGuiTableColumnFlags_WidthFixed, 62.0f);
        // BL-449: the stance column. Wide enough for the label plus the two/three
        // presses it carries — Declare Hostile / Offer / Accept / Return to Neutral.
        ImGui::TableSetupColumn("Stance",  ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableHeadersRow();

        for (const corp_standing& cs : standings)
        {
            const entity_id id = cs.corp;
            const corporation_component& cc = w.corporations.at(id);

            ImGui::TableNextRow();

            if (cs.is_player)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                    IM_COL32(255, 255, 255, 40));

            // Selectable spanning all columns via the first; selecting routes full
            // detail (incl. home nation) to the Selection panel.
            ImGui::TableSetColumnIndex(0);
            const bool selected = (s.selected_entity == id);
            if (ImGui::Selectable(cc.name.c_str(), selected,
                    ImGuiSelectableFlags_SpanAllColumns))
            {
                s.selected_entity = id;
            }

            ImGui::TableSetColumnIndex(1);
            if (cs.is_player)
                ImGui::Text("%d bodies", cs.reach_bodies);
            else
                ImGui::TextUnformatted(standing_band_label(cs.reach_band));

            ImGui::TableSetColumnIndex(2);
            if (cs.is_player)
            {
                const ImU32 col = (cs.capital_balance < 0.0f) ? palette::negative : palette::positive;
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%.1f", cs.capital_balance);
            }
            else
            {
                ImGui::TextUnformatted(standing_band_label(cs.capital_band));
            }

            ImGui::TableSetColumnIndex(3);
            if (cs.is_player)
                ImGui::Text("%.0f%%", cs.market_share * 100.0f);
            else
                ImGui::TextUnformatted(standing_band_label(cs.share_band));

            // BL-449: stance column. Player's own row carries no presses — a corp
            // cannot stance itself (stance.hpp's valid_pair rejects from == to).
            ImGui::TableSetColumnIndex(4);
            if (cs.is_player || player == null_entity)
            {
                ImGui::TextDisabled("-");
            }
            else if (!corp_is_discovered(w, id))
            {
                // NR-350: a hostile declaration stays SILENT — discovered on
                // contact, not announced. So an undiscovered rival's stance
                // (including one it may already hold toward the player) reads
                // as unknown rather than being revealed here ahead of contact.
                ImGui::TextDisabled("Unknown");
            }
            else
            {
                ImGui::PushID(static_cast<int>(id));

                const char* label = stance_label(w, player, id);
                ImGui::TextUnformatted(label);
                ImGui::SameLine();

                const bool hostile = is_hostile(w, player, id);
                const bool friends = are_friends(w, player, id);
                const bool offer_pending_out = has_pending_friendship_offer(w, player, id);
                const bool offer_pending_in  = has_pending_friendship_offer(w, id, player);

                // Declare Hostile — demolish precedent (SELECTION.md's Dismantle):
                // not literally irreversible, but not unilaterally reversible by
                // the target either, so it earns the same confirm-popup friction.
                if (!hostile)
                {
                    if (ImGui::SmallButton("Declare Hostile"))
                        ImGui::OpenPopup("confirm_declare_hostile");

                    if (ImGui::BeginPopup("confirm_declare_hostile"))
                    {
                        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                            "Declare %s hostile?", cc.name.c_str());
                        ImGui::TextDisabled("Dissolves any friendship. Not unilaterally reversible.");
                        ImGui::Separator();
                        if (ImGui::Button("Declare"))
                        {
                            corp_command cmd;
                            cmd.corp        = player;
                            cmd.verb        = corp_verb::declare_hostile;
                            cmd.counterparty = id;
                            s.pending_order_commands.push_back(cmd);
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Cancel"))
                            ImGui::CloseCurrentPopup();
                        ImGui::EndPopup();
                    }
                    ImGui::SameLine();
                }

                if (!friends && !hostile)
                {
                    if (offer_pending_in)
                    {
                        // The rival already offered friendship to the player — accept it.
                        if (ImGui::SmallButton("Accept Friendship"))
                        {
                            corp_command cmd;
                            cmd.corp         = player; // acceptor
                            cmd.verb         = corp_verb::accept_friendship;
                            cmd.counterparty = id;      // the offerer
                            s.pending_order_commands.push_back(cmd);
                        }
                    }
                    else if (offer_pending_out)
                    {
                        ImGui::TextDisabled("Offer sent");
                    }
                    else
                    {
                        if (ImGui::SmallButton("Offer Friendship"))
                        {
                            corp_command cmd;
                            cmd.corp         = player;
                            cmd.verb         = corp_verb::offer_friendship;
                            cmd.counterparty = id;
                            s.pending_order_commands.push_back(cmd);
                        }
                    }
                    ImGui::SameLine();
                }

                if (hostile || friends)
                {
                    if (ImGui::SmallButton("Return to Neutral"))
                    {
                        corp_command cmd;
                        cmd.corp         = player;
                        cmd.verb         = corp_verb::return_to_neutral;
                        cmd.counterparty = id;
                        s.pending_order_commands.push_back(cmd);
                    }
                }

                ImGui::PopID();
            }
        }

        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextDisabled("Corporations: %d", static_cast<int>(w.corporations.size()));

    ui::foldout_end();
}

} // namespace ui
