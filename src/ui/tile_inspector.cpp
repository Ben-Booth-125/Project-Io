#include "tile_inspector.hpp"
#include "text_fit.hpp"

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

    // --- Top-level view strip (BL-211) ---
    // Three questions, one per tab: what happened here (Story), how the chain got
    // there (Chain), and what it left on the ground (Tiles). Two levels rather than
    // five tabs on one row because the shell column cannot fit five labels — and
    // because the Chain's own sub-strip is the wizard's round grouping, which the
    // player has already been taught once at generation.
    enum view_id { view_story = 0, view_chain, view_tiles };
    int& view = s.history_view; // in ui_state so a verify script can park a view

    nav_button("Story", view_story, view, p_open);
    ImGui::SameLine();
    nav_button("Chain", view_chain, view, p_open);
    ImGui::SameLine();
    nav_button("Tiles", view_tiles, view, p_open);

    // The view-level fold control (BL-214). Story and Tiles are single blocks, so
    // the chevron gives the whole view the screen — which is the honest fix for the
    // Tiles table, whose 29 columns have always overflowed a 380 px column. Chain
    // does NOT take one: its stages carry their own, and a second control governing
    // all four at once would re-merge what the per-stage fold separates.
    if (view != view_chain)
    {
        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX()
                        - ImGui::GetFrameHeight());
        fold_chevron(s, (view == view_story) ? detail_surface::history_story
                                             : detail_surface::history_tiles, 0);
    }
    ImGui::Separator();

    const body_component& sel_body = w.bodies.at(selected_body);

    // The Chain view charts every body side by side — that comparison IS the view —
    // so the per-body selector belongs to the two views that are about one body.
    if (view != view_chain && ImGui::BeginCombo("Body", sel_body.name.c_str()))
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
    if (view != view_chain)
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
        // Wrapping container (1): the round question clipped mid-word (BL-215).
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ui::wrap_text(ui::text_box::foldout_column, "tile_inspector.round_question",
                      cr.question, ImGui::GetContentRegionAvail().x);
        ImGui::PopStyleColor();
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
        return;
    }

    // --- What the chain left on the ground ---
    //
    // The per-tile table that used to open this view was REMOVED 2026-08-01 (Ben,
    // NEEDS_REVIEW NR-014): "I'm actually not a massive fan of the tiles table. This
    // is because it can be seen by looking at the canvas." It listed x, y,
    // composition, landform, hazard, habitability and all 23 deposits for every tile
    // on the body — 29 columns that never fitted the 380 px ledger, and every one of
    // which the Planetary canvas already shows spatially, where position is the point.
    // Recover it from git if it is ever wanted as a debug view; it is not a UI.
    //
    // Expanding (BL-214) still opens the overlay HERE so everything below draws into
    // the full screen without being restated.
    const bool tiles_full =
        ui::fold_overlay_begin(s, detail_surface::history_tiles, 0, sel_body.name.c_str());

    // --- Buildings on this body ---
    ImGui::Spacing();
    ImGui::SeparatorText("Buildings");

    bool any_buildings = false;
    for (const auto& [id, bld] : w.buildings)
    {
        // Resolve the building's tile to check body ownership.
        auto tile_it = w.tiles.find(bld.tile);
        if (tile_it == w.tiles.end() || tile_it->second.body != selected_body)
            continue;

        any_buildings = true;
        const tile_component& t = tile_it->second;
        ImGui::BulletText("%s  at [%d,%d]  workforce %.0f%%",
            building_type_name(bld.type),
            t.grid_x, t.grid_y,
            bld.workforce_assigned * 100.0f);
    }
    if (!any_buildings)
        ImGui::TextDisabled("None");

    // --- Market state for this body ---
    ImGui::Spacing();
    ImGui::SeparatorText("Market");

    bool any_market = false;
    for (const auto& [id, mkt] : w.markets)
    {
        if (mkt.body != selected_body)
            continue;

        any_market = true;

        // Section comment: columns mirror market_component arrays so this
        // panel is the functional specification for the production market ledger.
        if (ImGui::BeginTable("market", 5,
            ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_RowBg        | ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("Resource",   ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Supply",     ImGuiTableColumnFlags_WidthFixed,  70.0f);
            ImGui::TableSetupColumn("Demand",     ImGuiTableColumnFlags_WidthFixed,  70.0f);
            ImGui::TableSetupColumn("Price",      ImGuiTableColumnFlags_WidthFixed,  60.0f);
            ImGui::TableSetupColumn("Base Price", ImGuiTableColumnFlags_WidthFixed,  80.0f);
            ImGui::TableHeadersRow();

            for (std::size_t r = 0; r < resource_count; ++r)
            {
                const resource_presentation& rp = presentation_of(static_cast<resource_type>(r));
                ImGui::TableNextRow();

                // Resource: identity colour swatch + name, drawn from the shared
                // presentation metadata so the colour reads the same everywhere.
                ImGui::TableSetColumnIndex(0);
                const float    sw   = ImGui::GetTextLineHeight();
                const ImVec2   swp  = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(swp, {swp.x + sw, swp.y + sw}, rp.colour);
                ImGui::Dummy({sw, sw});
                ImGui::SameLine();
                ImGui::TextUnformatted(rp.name);

                ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f", mkt.supply[r]);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%.1f", mkt.demand[r]);

                // Price coloured by its move against the base: above base reads as
                // a gain (sold dear), below as a loss. Neutral until the first
                // economy tick moves prices off base_price.
                ImGui::TableSetColumnIndex(3);
                const float move = mkt.price[r] - mkt.base_price[r];
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(value_colour(move)),
                                   "%.2f", mkt.price[r]);

                ImGui::TableSetColumnIndex(4); ImGui::Text("%.2f", mkt.base_price[r]);
            }
            ImGui::EndTable();
        }
        break; // one market per body
    }
    if (!any_market)
        ImGui::TextDisabled("No market.");

    if (tiles_full)
        ui::fold_overlay_end(s);
    ui::foldout_end();
}

} // namespace ui
