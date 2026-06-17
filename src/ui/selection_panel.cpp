#include "selection_panel.hpp"

#include "entity_summary.hpp"
#include "presentation.hpp"
#include "selection.hpp"
#include "view_nav.hpp"

#include "world/market_clearing.hpp"
#include "world/placement_rules.hpp"

#include <imgui.h>

namespace ui {

namespace {

// --- Build front door (tile Selection element) -------------------------------
// The per-tile entry to construction (docs/ui/SELECTION.md): offers the buildable
// types + cost and, on click, enqueues a construction request on this tile for the
// player corporation. The request is executed by app against the mutable world —
// here we only read and enqueue. Reached only for a selected tile.
void draw_build_front_door(const world& w, const recipe_registry& reg,
                           ui_state& ui, entity_id tile)
{
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end())
        return;
    const tile_component& tc = tit->second;

    ImGui::Separator();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Build here");

    if (placement_rules::is_ocean_tile(tc.composition))
    {
        ImGui::TextDisabled("Cannot build on water.");
        return;
    }

    // Player balance gates affordability (a building the corp cannot pay for is
    // offered disabled, so the cost is still legible).
    float balance = 0.0f;
    const auto pit = w.corporations.find(w.player_entity);
    if (pit != w.corporations.end())
        balance = pit->second.balance;

    // Enqueue helper — sets the pending request app executes this frame.
    auto enqueue = [&ui, tile](building_type type, resource_type target) {
        ui.construction.pending_tile   = tile;
        ui.construction.pending_type   = type;
        ui.construction.pending_target = target;
    };

    // --- Extraction: only the extractable resources actually deposited here ---
    bool any_extractable = false;
    for (const resource_type r : placement_rules::k_extractable)
        if (tc.resource_deposit[static_cast<std::size_t>(r)] > 0.0f)
            any_extractable = true;

    if (any_extractable)
    {
        // A compact target picker over the deposits present, writing construction.target.
        if (!placement_rules::is_extractable(ui.construction.target) ||
            tc.resource_deposit[static_cast<std::size_t>(ui.construction.target)] <= 0.0f)
        {
            // Default the target to the first present extractable so the button is valid.
            for (const resource_type r : placement_rules::k_extractable)
                if (tc.resource_deposit[static_cast<std::size_t>(r)] > 0.0f)
                { ui.construction.target = r; break; }
        }
        for (const resource_type r : placement_rules::k_extractable)
        {
            if (tc.resource_deposit[static_cast<std::size_t>(r)] <= 0.0f)
                continue;
            const bool sel = (ui.construction.target == r);
            if (ImGui::RadioButton(resource_name(r), sel))
                ui.construction.target = r;
            ImGui::SameLine();
        }
        ImGui::NewLine();

        const float cost = reg.economics(building_type::extraction_site).build_cost;
        ImGui::BeginDisabled(balance < cost);
        if (ImGui::Button("Build extraction site"))
            enqueue(building_type::extraction_site, ui.construction.target);
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("(%.0f)", cost);
    }
    else
    {
        ImGui::TextDisabled("No extractable deposit here.");
    }

    // --- Processing facility + Port: any non-ocean land tile ---
    const float pc = reg.economics(building_type::processing_facility).build_cost;
    ImGui::BeginDisabled(balance < pc);
    if (ImGui::Button("Build processing facility"))
        enqueue(building_type::processing_facility, resource_type::iron_ore);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("(%.0f)", pc);

    const float portc = reg.economics(building_type::port).build_cost;
    ImGui::BeginDisabled(balance < portc);
    if (ImGui::Button("Build port"))
        enqueue(building_type::port, resource_type::iron_ore);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("(%.0f)", portc);

    // Outcome of the most recent attempt, if any.
    if (!ui.construction.last_message.empty())
        ImGui::TextDisabled("%s", ui.construction.last_message.c_str());
}

// Headline label for a selected entity. Bodies carry a name; the other kinds
// have none, so their kind doubles as the title and the stat block supplies the
// distinguishing detail (coordinates, host body, etc.).
const char* selection_title(const world& w, selection_kind kind, entity_id id)
{
    switch (kind)
    {
        case selection_kind::body:        return w.bodies.at(id).name.c_str();
        case selection_kind::building:    return building_type_name(w.buildings.at(id).type);
        case selection_kind::tile:        return "Tile";
        case selection_kind::market:      return "Market";
        case selection_kind::unit:        return "Unit";
        case selection_kind::nation:      return w.nations.at(id).name.c_str();
        case selection_kind::corporation: return w.corporations.at(id).name.c_str();
        case selection_kind::none:        return "Nothing";
    }
    return "?";
}

// Dispatch to the matching shared content builder (entity_summary.hpp).
void draw_summary(const world& w, selection_kind kind, entity_id id)
{
    switch (kind)
    {
        case selection_kind::body:        draw_body_summary(w, id);        break;
        case selection_kind::tile:        draw_tile_summary(w, id);        break;
        case selection_kind::building:    draw_building_summary(w, id);    break;
        case selection_kind::market:      draw_market_summary(w, id);      break;
        case selection_kind::unit:        draw_unit_summary(w, id);        break;
        case selection_kind::nation:      draw_nation_summary(w, id);      break;
        case selection_kind::corporation: draw_corporation_summary(w, id); break;
        case selection_kind::none:        break;
    }
}

} // namespace

void draw_selection_panel(const world& w, const recipe_registry& reg, ui_state& ui,
                          float left_x, float bottom_y)
{
    const selection_kind kind = selection_kind_of(w, ui.selected_entity);

    // Hidden when nothing valid is selected, or when the player dismissed this
    // exact selection (close hides until the next selection re-shows it).
    if (kind == selection_kind::none)
        return;
    if (ui.selected_entity == ui.selection_hidden_for)
        return;

    ImGui::SetNextWindowPos({left_x, bottom_y}, ImGuiCond_Always, {0.0f, 1.0f});
    ImGui::SetNextWindowSize({320.0f, 0.0f}, ImGuiCond_Always); // fixed width, auto height
    ImGui::SetNextWindowBgAlpha(0.85f);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar            |
        ImGuiWindowFlags_NoResize              |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoCollapse            |
        ImGuiWindowFlags_NoNav                 |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##selection_info", nullptr, flags);

    // --- Header: title + kind on the left, 'go to' and close on the right ---
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                       "%s", selection_title(w, kind, ui.selected_entity));
    ImGui::SameLine();
    ImGui::TextDisabled("%s", selection_kind_name(kind));

    // Right-align the two square buttons against the inner edge.
    const float        btn   = ImGui::GetFrameHeight();
    const ImGuiStyle&  style = ImGui::GetStyle();
    const float        right = ImGui::GetWindowWidth() - style.WindowPadding.x;
    ImGui::SameLine(right - 2.0f * btn - style.ItemSpacing.x);

    // 'Go to' — same effect as a double-click on the selection. focus_on_entity
    // resolves it to the most informative view (or, later, a ledger).
    if (ImGui::Button(">", {btn, btn}))
        focus_on_entity(w, ui, ui.selected_entity);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Go to");
    ImGui::SameLine();

    // Close — hides until the next selection; the panel is not destroyed.
    if (ImGui::Button("x", {btn, btn}))
        ui.selection_hidden_for = ui.selected_entity;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Close");

    ImGui::Separator();

    // --- Body: polymorphic content for the selected kind ---
    draw_summary(w, kind, ui.selected_entity);

    // Lens-contextual supplement (BL-032): when a tile is selected and a lens is
    // active, show the most relevant stat for that lens below the base summary.
    if (kind == selection_kind::tile)
    {
        const entity_id tile = ui.selected_entity;
        const auto tit = w.tiles.find(tile);

        if (tit != w.tiles.end())
        {
            if (ui.overlay == overlay_mode::corporation)
            {
                // Corporation lens: show which corp owns a building on this tile.
                const corporation_component* owner_corp = nullptr;
                for (const auto& [bld_id, bc] : w.buildings)
                {
                    if (bc.tile != tile)
                        continue;
                    for (const auto& [corp_id, cc] : w.corporations)
                    {
                        for (entity_id a : cc.assets)
                        {
                            if (a == bld_id)
                            {
                                owner_corp = &cc;
                                break;
                            }
                        }
                        if (owner_corp)
                            break;
                    }
                    break;
                }
                if (owner_corp)
                {
                    ImGui::Separator();
                    ImGui::TextColored({0.7f, 0.8f, 1.0f, 1.0f},
                        "Owner: %s", owner_corp->name.c_str());
                }
            }
            else if (ui.overlay == overlay_mode::market)
            {
                // Market lens: show which market's catchment covers this tile.
                ImGui::Separator();
                const entity_id mkt_id = market_for_tile(w, tile);
                const auto mkt_it = w.markets.find(mkt_id);
                if (mkt_it != w.markets.end())
                {
                    // Ordinal index for display (stable relative to iteration order).
                    int idx = 1;
                    for (const auto& [mid, mk] : w.markets)
                    {
                        if (mk.body != tit->second.body)
                            continue;
                        if (mid == mkt_id)
                            break;
                        ++idx;
                    }
                    ImGui::Text("Market %d catchment", idx);
                    // Show selected-resource price.
                    const std::size_t g = static_cast<std::size_t>(ui.lens_resource);
                    ImGui::Text("%s price: %.2f",
                                resource_name(ui.lens_resource),
                                static_cast<double>(mkt_it->second.price[g]));
                }
            }
            else if (ui.overlay == overlay_mode::production)
            {
                // Production lens: show base output of any building on this tile.
                for (const auto& [bld_id, bc] : w.buildings)
                {
                    if (bc.tile != tile)
                        continue;
                    ImGui::Separator();
                    const building_economics& eco = reg.economics(bc.type);
                    ImGui::Text("Base rate: %.1f / tick", static_cast<double>(eco.base_rate));
                    ImGui::Text("Workforce: %.0f%%",
                                static_cast<double>(bc.workforce_assigned) * 100.0);
                    break;
                }
            }
        }
    }

    // Build front door — only for a selected tile (the per-tile construction entry).
    if (kind == selection_kind::tile)
        draw_build_front_door(w, reg, ui, ui.selected_entity);

    ImGui::End();
}

} // namespace ui
