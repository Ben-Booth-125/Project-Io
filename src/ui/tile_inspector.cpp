#include "tile_inspector.hpp"

#include "foldout_column.hpp"    // shell fold-out column host (BL-122/BL-144)
#include "format.hpp"            // campaign_epoch_year, cross-checked below (BL-220)
#include "generation_charts.hpp" // the chain-stage charts, shared with the wizard (BL-211)
#include "presentation.hpp"
#include "world/components.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace ui {

// The tick calendar and the generated history count from the same year, and
// nothing else forces them to agree — the world layer cannot include a UI
// header, so ::campaign_epoch_year (planetology.hpp) is a deliberate duplicate.
// This is the only place both are in scope, so the guard lives here.
static_assert(fmt::campaign_epoch_year == static_cast<int>(::campaign_epoch_year),
              "History dates and the tick calendar must share a campaign epoch");

void draw_tile_inspector(const world& w, ui_state& s,
                         const generation_report& report, bool* p_open)
{
    // Honour the open flag; when closed the window draws nothing at all.
    if (p_open && !*p_open)
        return;

    // Collect body IDs into a stable order for the combo box.
    std::vector<entity_id> body_ids;
    body_ids.reserve(w.bodies.size());
    for (const auto& [id, _] : w.bodies)
        body_ids.push_back(id);
    std::sort(body_ids.begin(), body_ids.end());

    // Re-hosted into the shell fold-out column (BL-144); closed via the nav rail.
    if (!ui::foldout_begin("Tile Ledger"))
    {
        ui::foldout_end();
        return;
    }

    if (body_ids.empty())
    {
        ImGui::TextDisabled("No bodies in world.");
        ui::foldout_end();
        return;
    }

    // --- Body selector ---
    // Default: prefer the canvas's active surface body, then fall back to lowest id.
    static entity_id selected_body = null_entity;
    if (selected_body == null_entity || w.bodies.find(selected_body) == w.bodies.end())
    {
        // 1. Use ui_state.active_body when it refers to a surface body.
        if (s.active_body != null_entity)
        {
            auto ab_it = w.bodies.find(s.active_body);
            if (ab_it != w.bodies.end() && ab_it->second.type != body_type::star)
            {
                selected_body = s.active_body;
            }
        }
        // 2. Fall back to the lowest-id surface body (existing default).
        if (selected_body == null_entity || w.bodies.find(selected_body) == w.bodies.end())
            selected_body = body_ids.front();
    }

    // --- Top-level view strip (BL-211, narrowed by BL-281) ---
    // Two questions, one per tab, sharing one premise — how this world came to be:
    // what happened here (Story) and how the chain got there (Chain). The third tab,
    // Tiles, was RETIRED 2026-08-03 (Ben, NEEDS_REVIEW NR-020 option 2): it was a
    // current-state readout inside a ledger about the past, and renaming it would
    // have fixed the label while keeping the defect.
    enum view_id { view_story = 0, view_chain };
    int& view = s.history_view; // in ui_state so a verify script can park a view

    // Clamp: a script (or a stale saved index) can still hand us the retired third
    // view, and neither branch below would then draw anything.
    if (view < view_story || view > view_chain)
        view = view_story;

    nav_button("Story", view_story, view, p_open);
    ImGui::SameLine();
    nav_button("Chain", view_chain, view, p_open);

    // The view-level fold control (BL-214). Story is a single block, so the chevron
    // gives the whole view the screen. Chain does NOT take one: its stages carry
    // their own, and a second control governing all four at once would re-merge what
    // the per-stage fold separates.
    if (view == view_story)
    {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX()
                        - ImGui::GetFrameHeight());
        fold_chevron(s, detail_surface::history_story, 0);
    }
    ImGui::Separator();

    const body_component& sel_body = w.bodies.at(selected_body);

    // The Chain view charts every body side by side — that comparison IS the view —
    // so the per-body selector belongs to Story, the view that is about one body.
    if (view == view_story && ImGui::BeginCombo("Body", sel_body.name.c_str()))
    {
        for (entity_id id : body_ids)
        {
            const body_component& b = w.bodies.at(id);
            const bool is_sel = (id == selected_body);
            if (ImGui::Selectable(b.name.c_str(), is_sel))
                selected_body = id;
            if (is_sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Body summary line
    if (view == view_story)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s  |  %.2f AU  |  %dx%d tiles",
            body_type_name(sel_body.type),
            sel_body.orbital_radius_au,
            sel_body.grid_width,
            sel_body.grid_height);
        ImGui::Separator();
    }

    // The report is matched by NAME (generation_report::body_entry has no
    // entity_id — bodies are authored once and never renamed, so name is a stable
    // key here).
    const generation_report::body_entry* entry = nullptr;
    for (const auto& be : report.bodies)
        if (be.name == sel_body.name) { entry = &be; break; }

    // --- Story: the oral-history biography (BL-211) ---
    // GENERATION_LEDGER.md flagged this as an open item ("player-facing
    // variant... out of scope") and MENU.md named this slot as its destination
    // ("generation history, not a live event log").
    if (view == view_story)
    {
        // Drawn into whichever host is active — the column at rest, the full-screen
        // overlay when expanded — rather than twice. One body, two hosts, so the two
        // cannot show different things.
        auto story_body = [&]() {
            if (!entry || entry->state.history.empty())
            {
                ImGui::TextDisabled("No recorded history for this body.");
                return;
            }
            ImGui::TextDisabled("%s - %s to present",
                archetype_name(entry->state.archetype),
                format_history_date(entry->state.history.front().years_before_epoch).c_str());
            ImGui::Spacing();
            for (const history_event& ev : entry->state.history)
            {
                // The date's unit comes from its own magnitude, so a deep-time
                // line and a historical one share this loop (BL-220).
                ImGui::TextWrapped("%s  %s",
                    format_history_date(ev.years_before_epoch).c_str(), ev.event.c_str());
                if (!ev.consequence.empty())
                {
                    // Wrapped, not TextDisabled: the consequence is the longer half
                    // of the pair and the column clipped it mid-word.
                    ImGui::Indent();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
                    ImGui::TextWrapped("%s", ev.consequence.c_str());
                    ImGui::PopStyleColor();
                    ImGui::Unindent();
                }
            }
        };

        if (ui::fold_overlay_begin(s, detail_surface::history_story, 0, sel_body.name.c_str()))
        {
            story_body();
            ui::fold_overlay_end(s);
        }
        else
        {
            story_body();
        }
        ui::foldout_end();
        return;
    }

    // --- Chain: the generation charts, one accordion per stage (BL-211) ---
    // These are the wizard's own plots (ui::generation_charts), redrawn from the
    // persisted report. Before this they existed only on the screen the player
    // clicks through once — the richest visuals in the game, discarded at the
    // moment the world opened. The round strip is the wizard's grouping, so a
    // player who read the chain at generation finds it filed the way they learned
    // it; the stages inside collapse so one question is open at a time.
    if (view == view_chain)
    {
        if (report.bodies.empty())
        {
            ImGui::TextDisabled("No generation report - this world predates the chain.");
            ui::foldout_end();
            return;
        }

        int& round = s.history_round;
        nav_button("System", 0, round);
        ImGui::SameLine();
        nav_button("Life", 1, round);
        ImGui::SameLine();
        nav_button("Legacy", 2, round);

        const ui::chain_round& cr = ui::chain_round_at(round);
        ImGui::TextDisabled("%s", cr.question);
        ImGui::Separator();

        // Rebuilt per frame from the report: a handful of pointers, no copies of the
        // states themselves.
        std::vector<ui::generation_chart_body> chart_bodies;
        chart_bodies.reserve(report.bodies.size());
        std::size_t home = 0;
        for (std::size_t i = 0; i < report.bodies.size(); ++i)
        {
            const generation_report::body_entry& be = report.bodies[i];
            chart_bodies.push_back(ui::generation_chart_body{
                be.name.c_str(), &be.state, &be.undrawn });
            // The homeworld is the only body S9 runs on, so a non-zero drawdown is
            // what identifies it without the report carrying a flag.
            if (be.state.drawdown > 0.0f)
                home = i;
        }
        const ui::generation_chart_source src{
            chart_bodies.data(), chart_bodies.size(), home };

        // Each stage rests as its verdict line and expands full screen (BL-214).
        // This replaces the per-stage CollapsingHeader, which was this surface's own
        // private disclosure idiom — the fourth one the item exists to retire. It
        // also fixes what the accordion could not: four stages of charts never fit
        // this 380 px column, so "open" here always meant "scroll", while the
        // overlay gives every chart the width it was drawn for.
        for (int si = static_cast<int>(cr.first); si <= static_cast<int>(cr.last); ++si)
            ui::draw_stage_fold(src, static_cast<chain_stage>(si), s,
                                detail_surface::history_chain);

        ui::foldout_end();
    }
}

} // namespace ui
