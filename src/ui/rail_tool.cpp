#include "rail_tool.hpp"

#include "presentation.hpp"
#include "ui_state.hpp"

#include "world/construction.hpp"     // track_tile_cost, construction_capex
#include "world/market_clearing.hpp"  // market_for_tile
#include "world/recipe_registry.hpp"
#include "world/state_industry.hpp"   // state_uncommitted_treasury

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <map>

namespace ui
{

namespace
{

bool is_city_tile(const world& w, entity_id tile)
{
    for (const auto& [centre, ctile] : w.population_centre_tile)
    {
        (void)centre;
        if (ctile == tile)
            return true;
    }
    return false;
}

/// Whether a cardinal neighbour of @p tile (or, with @p self, the tile itself)
/// carries track — owned by @p owner when @p owner is set, by anyone otherwise.
bool touches_track(const world& w, entity_id tile, entity_id owner, bool self)
{
    if (self && w.rail_track.count(tile) != 0)
        return true;
    for (const auto& [t, o] : w.rail_track)
        if ((owner == null_entity || o == owner) && rail_tiles_adjacent(w, tile, t))
            return true;
    return false;
}

const ImVec4 k_refusal{0.90f, 0.55f, 0.55f, 1.0f};

} // namespace

entity_id rail_player_nation(const world& w)
{
    const auto it = w.corporations.find(w.player_entity);
    if (it == w.corporations.end())
        return null_entity;
    const entity_id n = it->second.home_nation;
    if (w.nations.find(n) == w.nations.end() || champion_of(w, n) != w.player_entity)
        return null_entity;
    return n;
}

bool rail_tiles_adjacent(const world& w, entity_id a, entity_id b)
{
    const auto ia = w.tiles.find(a), ib = w.tiles.find(b);
    if (ia == w.tiles.end() || ib == w.tiles.end() || ia->second.body != ib->second.body)
        return false;
    const auto bit = w.bodies.find(ia->second.body);
    if (bit == w.bodies.end())
        return false;
    const int gw = bit->second.grid_width;
    const int dy = std::abs(ia->second.grid_y - ib->second.grid_y);
    int dx = std::abs(ia->second.grid_x - ib->second.grid_x);
    if (gw > 0)
        dx = std::min(dx, gw - dx); // columns wrap
    return dx + dy == 1;
}

bool rail_track_tile_ok(const world& w, entity_id nation, entity_id tile,
                        const std::vector<entity_id>& run, const char** why)
{
    const auto fail = [&](const char* m) { if (why) *why = m; return false; };
    const auto tit = w.tiles.find(tile);
    if (nation == null_entity)
        return fail("No major nation is seated");
    if (tit == w.tiles.end())
        return fail("No tile");
    if (is_water(tit->second.substrate))
        return fail("Track is laid on land");
    if (w.rail_track.count(tile) != 0)
        return fail("Already carries track");
    if (std::find(run.begin(), run.end(), tile) != run.end())
        return fail("Already planned");
    const auto tn = w.tile_to_nation.find(tile);
    if (tn == w.tile_to_nation.end() || tn->second != nation)
        return fail("Not your nation's ground");
    if (run.empty())
    {
        if (!is_city_tile(w, tile) && !touches_track(w, tile, nation, false))
            return fail("Start at a city or your own line");
        return true;
    }
    if (!rail_tiles_adjacent(w, run.back(), tile))
        return fail("Must touch the run's last tile");
    return true;
}

bool rail_station_tile_ok(const world& w, entity_id nation, entity_id tile, const char** why)
{
    const auto fail = [&](const char* m) { if (why) *why = m; return false; };
    if (nation == null_entity)
        return fail("No major nation is seated");
    if (w.tiles.find(tile) == w.tiles.end())
        return fail("No tile");
    const auto tn = w.tile_to_nation.find(tile);
    if (tn == w.tile_to_nation.end() || tn->second != nation)
        return fail("Not your nation's ground");
    if (!is_city_tile(w, tile))
        return fail("A station stands in a city");
    if (!touches_track(w, tile, null_entity, true))
        return fail("Must touch track");
    for (const auto& [bid, b] : w.buildings)
    {
        (void)bid;
        if (b.type == building_type::railway_station && b.tile == tile)
            return fail("A station stands here");
    }
    return true;
}

void draw_rail_group(const world& w, const recipe_registry& reg, ui_state& ui)
{
    construction_state&    cs = ui.construction;
    construction_controls& cc = ui.construction_ui;

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Rail");
    ImGui::SameLine();
    ImGui::TextDisabled("state industry");

    const entity_id nation = rail_player_nation(w);
    if (nation == null_entity)
    {
        ImGui::TextDisabled("Rail is laid by a major nation's state.");
        cs.rail = rail_tool::none;
        cs.rail_run.clear();
        return;
    }
    if (!reg.building_available(building_type::railway_station))
    {
        ImGui::TextDisabled("No railway in this era.");
        cs.rail = rail_tool::none;
        cs.rail_run.clear();
        return;
    }

    const float treasury = state_uncommitted_treasury(w, nation);
    ImGui::TextDisabled("Treasury: %.0f cr uncommitted", static_cast<double>(treasury));

    // The two tools. Each is a TOGGLE (the standing rule): pressing the armed
    // tool disarms it; pressing the other switches. Arming either disarms
    // building placement, so one gesture owns the canvas click.
    const auto tool_button = [&](const char* label, rail_tool t, float& px, float& py) {
        const bool on = cs.rail == t;
        if (on)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        const ImVec2 p = ImGui::GetCursorScreenPos();
        if (ImGui::Button(label))
        {
            cs.rail = on ? rail_tool::none : t;
            cs.active = false;
            cs.rail_message.clear();
        }
        px = p.x + ImGui::GetItemRectSize().x * 0.5f;
        py = p.y + ImGui::GetItemRectSize().y * 0.5f;
        if (on)
            ImGui::PopStyleColor();
    };
    tool_button("Lay track", rail_tool::track, cc.rail_track_x, cc.rail_track_y);
    ImGui::SameLine();
    tool_button("Railway station", rail_tool::station, cc.rail_station_x, cc.rail_station_y);

    if (cs.rail != rail_tool::track && !cs.rail_run.empty())
        cs.rail_run.clear(); // a disarmed tool plans nothing

    struct { float steel, upkeep; } const rp{reg.rail().track_steel_per_tile, reg.rail().track_upkeep_per_tile};

    if (cs.rail == rail_tool::track)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("Click or drag tiles on the map; click the last tile to undo it.");
        ImGui::TextWrapped("Per tile: %.0f cr + %.1f steel bought at the tile market; upkeep %.2f cr/tick",
                            static_cast<double>(reg.rail().track_build_cost),
                            static_cast<double>(rp.steel), static_cast<double>(rp.upkeep));
        ImGui::PopStyleColor();

        // Price the run as the door would, tile by tile in lay order: the
        // treasury shrinks, and each market's shelf gives up its steel.
        float total = 0.0f;
        int   n_ok  = 0;
        const char* first_refusal = nullptr;
        std::map<entity_id, float> shelf_used;
        for (const entity_id t : cs.rail_run)
        {
            const float c = track_tile_cost(w, reg, t);
            const entity_id mid = market_for_tile(w, t);
            float shelf = 0.0f;
            if (const auto mit = w.markets.find(mid); mit != w.markets.end())
                shelf = mit->second.inventory[static_cast<std::size_t>(resource_type::steel)];
            float& used = shelf_used[mid];
            if (first_refusal == nullptr && rp.steel > 0.0f && !(shelf - used >= rp.steel))
                first_refusal = "No steel on the shelf";
            if (first_refusal == nullptr && (!std::isfinite(c) || total + c > treasury))
                first_refusal = "Treasury cannot cover it";
            if (first_refusal == nullptr)
            {
                used  += rp.steel;
                total += c;
                ++n_ok;
            }
        }
        const int n = static_cast<int>(cs.rail_run.size());
        ImGui::Text("Planned: %d tile%s", n, n == 1 ? "" : "s");
        ImGui::Text("Total: %.0f cr, %.1f steel; upkeep %.2f cr/tick",
                    static_cast<double>(total), static_cast<double>(rp.steel * n_ok),
                    static_cast<double>(rp.upkeep * n_ok));
        if (first_refusal != nullptr)
            ImGui::TextColored(k_refusal, "%s after %d tile%s", first_refusal, n_ok,
                               n_ok == 1 ? "" : "s");

        ImGui::BeginDisabled(n_ok == 0);
        {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            if (ImGui::Button("Lay planned track"))
                cs.pending_rail_commit = true;
            cc.rail_commit_x = p.x + ImGui::GetItemRectSize().x * 0.5f;
            cc.rail_commit_y = p.y + ImGui::GetItemRectSize().y * 0.5f;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(n == 0);
        if (ImGui::Button("Clear"))
            cs.rail_run.clear();
        ImGui::EndDisabled();
    }
    else if (cs.rail == rail_tool::station)
    {
        const building_economics& ev = reg.economics(building_type::railway_station);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("Click a ringed city tile on the map. Upkeep %.1f cr/tick once standing.",
                           static_cast<double>(ev.maintenance));
        ImGui::PopStyleColor();
        if (w.tiles.find(cs.rail_hover) != w.tiles.end())
        {
            const char* why = nullptr;
            if (rail_station_tile_ok(w, nation, cs.rail_hover, &why))
                ImGui::Text("Here: %.0f cr", static_cast<double>(construction_capex(
                    w, reg, cs.rail_hover, building_type::railway_station,
                    resource_type::iron_ore)));
            else
                ImGui::TextColored(k_refusal, "%s", why);
        }
    }

    if (!cs.rail_message.empty())
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::neutral), "%s",
                           cs.rail_message.c_str());
}

} // namespace ui
