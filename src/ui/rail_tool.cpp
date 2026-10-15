#include "rail_tool.hpp"

#include "icons.hpp"
#include "presentation.hpp"
#include "ui_state.hpp"

#include "world/construction.hpp"     // track_tile_cost, construction_capex
#include "world/market_clearing.hpp"  // market_for_tile
#include "world/recipe_registry.hpp"
#include "world/state_industry.hpp"   // state_uncommitted_treasury

#include <imgui.h>

#include <algorithm>
#include <cmath>

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

void draw_rail_rows(const world& w, const recipe_registry& reg, ui_state& ui, entity_id tile,
                    float row_h, rail_row which)
{
    construction_controls& cc = ui.construction_ui;
    const entity_id nation = rail_player_nation(w);
    // Rail is the seat NATION's state industry: no major seated, or no railway
    // in this era, and the rows are absent as an era-locked type is absent.
    if (nation == null_entity || !reg.building_available(building_type::railway_station)
        || w.tiles.find(tile) == w.tiles.end())
        return;

    const float treasury = state_uncommitted_treasury(w, nation);
    const auto  row = [&](const char* name, float credits, float steel, float upkeep,
                          const char* refusal, bool& pressed, float& px, float& py) {
        ImGui::BeginChild(name, {0.0f, row_h}, true,
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
        {
            constexpr float gr = 9.0f;
            const ImVec2    gp = ImGui::GetCursorScreenPos();
            icons::building(ImGui::GetWindowDrawList(),
                            {gp.x + gr, gp.y + ImGui::GetTextLineHeight() * 0.5f}, gr,
                            building_type::railway_station, resource_type::steel,
                            IM_COL32(150, 235, 160, 255));
            ImGui::Dummy({gr * 2.0f, ImGui::GetTextLineHeight()});
            ImGui::SameLine();
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", name);
        }
        if (steel > 0.0f)
            ImGui::TextDisabled("%.0f cr + %.1f steel", static_cast<double>(credits),
                                static_cast<double>(steel));
        else
            ImGui::TextDisabled("%.0f cr", static_cast<double>(credits));
        ImGui::TextDisabled("From the treasury (%.0f cr)", static_cast<double>(treasury));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::neutral),
                           "State industry - upkeep %.2f cr/tick", static_cast<double>(upkeep));
        if (refusal == nullptr)
        {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            if (ImGui::Button("Build"))
                pressed = true;
            // Published only while the press is on screen: the rows sit in a
            // scrolled list, and a position a click cannot reach is no press.
            if (ImGui::IsItemVisible())
            {
                px = p.x + ImGui::GetItemRectSize().x * 0.5f;
                py = p.y + ImGui::GetItemRectSize().y * 0.5f;
            }
        }
        else
            ImGui::TextColored(k_refusal, "%s", refusal);
        ImGui::EndChild();
        ImGui::Spacing();
    };

    // Track: the lay_track rule (own ground; a city of the nation, or touching
    // its own line), then the two purchases the door makes - steel off the
    // tile market's shelf, credits off the treasury.
    if (which == rail_row::track)
    {
        const char* why   = nullptr;
        const float steel = reg.rail().track_steel_per_tile;
        const float cost  = track_tile_cost(w, reg, tile);
        if (rail_track_tile_ok(w, nation, tile, {}, &why))
        {
            const auto mit = w.markets.find(market_for_tile(w, tile));
            const float shelf = mit == w.markets.end()
                ? 0.0f : mit->second.inventory[static_cast<std::size_t>(resource_type::steel)];
            if (steel > 0.0f && !(shelf >= steel))
                why = "No steel on the shelf";
            else if (!std::isfinite(cost) || cost > treasury)
                why = "Treasury cannot cover it";
        }
        bool pressed = false;
        row("Track", std::isfinite(cost) ? cost : 0.0f, steel, reg.rail().track_upkeep_per_tile,
            why, pressed, cc.rail_track_x, cc.rail_track_y);
        if (pressed)
            ui.construction.pending_track_tile = tile;
    }
    // Railway station: a city tile touching track, one to a tile.
    if (which == rail_row::station)
    {
        const char* why  = nullptr;
        const float cost = construction_capex(w, reg, tile, building_type::railway_station,
                                              resource_type::iron_ore);
        if (rail_station_tile_ok(w, nation, tile, &why) && (!std::isfinite(cost) || cost > treasury))
            why = "Treasury cannot cover it";
        bool pressed = false;
        row("Railway Station", std::isfinite(cost) ? cost : 0.0f, 0.0f,
            reg.economics(building_type::railway_station).maintenance, why, pressed,
            cc.rail_station_x, cc.rail_station_y);
        if (pressed)
            ui.construction.pending_station_tile = tile;
    }
    if (which == rail_row::track && !ui.construction.rail_message.empty())
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::neutral), "%s",
                           ui.construction.rail_message.c_str());
}

} // namespace ui
