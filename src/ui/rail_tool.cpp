#include "rail_tool.hpp"

#include "construction_panel.hpp" // the SHARED grid: columns, row metric, per-column cards
#include "format.hpp"             // fmt::abbreviate - the cost column's own figure
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
#include <cstdio>
#include <string>
#include <vector>

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

} // namespace

entity_id rail_player_nation(const world& w)
{
    // BL-1407: the player IS its nation.
    return player_nation(w);
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
            // BL-1379: THE GROUND REFUSAL, in its own words. This is the track row's
            // "nowhere to go" case (construction.md § "A tile with nowhere to go mutes
            // the press"), and it must not read like the treasury one below: no funding
            // ever makes a tile touch a line it does not touch.
            return fail("Nowhere to lay it from: a line starts at one of your nation's "
                        "cities, or touches track you already own");
        return true;
    }
    if (!rail_tiles_adjacent(w, run.back(), tile))
        return fail("Nowhere to lay it from: this tile does not touch the run's last one");
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

    // ONE LINE, IN THE SHARED GRID (BL-1366; construction.md § "The line is a
    // grid, not a sentence"). These two rows drew a four-line bordered box, so
    // Track and Railway Station sat visibly taller than the one-line rows above
    // and below them - the holding action was to hand them the OLD seven-line
    // metric so they would not clip. They are now the same code as the rows they
    // sit among: `construction_begin_build_grid`'s eight columns, the shared row
    // height, and `construction_column_card`'s per-column explainers. The refusal
    // that used to wrap to two red lines (BL-1255's live click) is the Build
    // press's own explainer, exactly as a refused building row's is.
    //
    // The row's own table rather than the caller's: a nested table would resolve
    // the stretch name column against one cell's width instead of the panel's, so
    // the columns would stop lining up - which is the whole point of the grid.
    const auto grid_row = [&](const char* id, const char* name, bool rail_glyph, float credits,
                              float steel, float upkeep, int here, int cap, const char* refusal,
                              bool& pressed, float& px, float& py) {
        if (!construction_begin_build_grid(id))
            return;
        const float line_h = ImGui::GetTextLineHeight();
        ImDrawList* dl     = ImGui::GetWindowDrawList();
        std::vector<grid_explainer> cells;
        ImGui::TableNextRow(ImGuiTableRowFlags_None, row_h);
        ImGui::PushID(id);

        // Column 0: the glyph, in the same 15 px box as a building row's.
        ImGui::TableSetColumnIndex(0);
        const ImVec2 gp = ImGui::GetCursorScreenPos();
        const float  gr = construction_grid::glyph * 0.5f;
        if (rail_glyph)
            icons::rail_line(dl, {gp.x + gr, gp.y + line_h * 0.5f}, gr * 0.76f,
                             IM_COL32(150, 235, 160, 255));
        else
            icons::building(dl, {gp.x + gr, gp.y + line_h * 0.5f}, gr,
                            building_type::railway_station, resource_type::steel,
                            IM_COL32(150, 235, 160, 255));
        ImGui::Dummy({construction_grid::glyph, line_h});

        // Column 1: the name.
        ImGui::TableSetColumnIndex(1);
        const float name_w = std::max(16.0f, ImGui::GetContentRegionAvail().x);
        // BL-1371: the same wrapping draw as every other name column, so these
        // rows keep being the SAME grid rather than two rows that resemble it.
        ImGui::PushStyleColor(ImGuiCol_Text, palette::selection);
        bool      name_elided = false;
        const int name_lines  = construction_grid_text("rail.row.name", name, name_w,
                                                       /*disabled=*/false, &name_elided);
        ImGui::PopStyleColor();
        cells.push_back(construction_cell(
            "name", gp, construction_grid::glyph + construction_grid::pad * 2.0f + name_w,
            std::string("State industry: the seat nation builds it, not your corporation.")));

        // Column 2: the cost, abbreviated as every other cost column is.
        ImGui::TableSetColumnIndex(2);
        const ImVec2 cp = ImGui::GetCursorScreenPos();
        ImGui::TextDisabled("%s", fmt::abbreviate(static_cast<double>(credits)).c_str());
        {
            char buf[280];
            if (steel > 0.0f)
                std::snprintf(buf, sizeof buf,
                              "Costs %d cr and %.1f Steel off this market's shelf. Both come "
                              "from the national treasury, which holds %d cr.",
                              static_cast<int>(credits), static_cast<double>(steel),
                              static_cast<int>(treasury));
            else
                std::snprintf(buf, sizeof buf,
                              "Costs %d cr from the national treasury, which holds %d cr.",
                              static_cast<int>(credits), static_cast<int>(treasury));
            cells.push_back(construction_cell("cost", cp, construction_grid::cost, buf));
        }

        // Column 3: the pip column stays EMPTY - rail makes no good - and says so.
        ImGui::TableSetColumnIndex(3);
        {
            const ImVec2 pp = ImGui::GetCursorScreenPos();
            ImGui::Dummy({construction_grid::makes, line_h});
            cells.push_back(construction_cell(
                "makes", pp, construction_grid::makes,
                rail_glyph ? std::string("Makes no good, so the pip is empty. Track is the line "
                                         "a railway runs on.")
                           : std::string("Makes no good, so the pip is empty. A station is where "
                                         "the railway loads and unloads.")));
        }

        // Column 4: the stack, as every other row's "here / cap".
        ImGui::TableSetColumnIndex(4);
        const ImVec2 sk = ImGui::GetCursorScreenPos();
        ImGui::TextDisabled("%d/%d", here, cap);
        cells.push_back(construction_cell(
            "stack", sk, construction_grid::stack,
            rail_glyph
                ? std::string("A tile carries one length of track, and this one ")
                      + (here > 0 ? "already does." : "does not yet.")
                : std::string("A city takes one station, and this one ")
                      + (here > 0 ? "already has it." : "has none.")));

        // Column 5: the state mark - grey when it will go up, red when refused.
        ImGui::TableSetColumnIndex(5);
        {
            const ImVec2 sp = ImGui::GetCursorScreenPos();
            icons::value_mark(dl, {sp.x + construction_grid::state * 0.5f, sp.y + line_h * 0.5f},
                              3.0f,
                              refusal == nullptr ? IM_COL32(110, 116, 128, 255)
                                                 : IM_COL32(230, 140, 140, 255));
            ImGui::Dummy({construction_grid::state, line_h});
            cells.push_back(construction_cell(
                "state", sp, construction_grid::state,
                refusal == nullptr
                    ? std::string("Nothing is holding it back: the treasury and the ground both "
                                  "allow it.")
                    : std::string("Red: this tile refuses it. The Build press says what refused "
                                  "it."),
                refusal == nullptr ? 0u : IM_COL32(230, 140, 140, 255)));
        }

        // Column 6: the profit bar stays EMPTY - state industry earns no corporate
        // revenue - and the upkeep sentence the old four lines carried lives here.
        ImGui::TableSetColumnIndex(6);
        {
            const ImVec2 bp = ImGui::GetCursorScreenPos();
            ImGui::Dummy({construction_grid::bar, line_h});
            char buf[240];
            std::snprintf(buf, sizeof buf,
                          "No corporate revenue: the nation owns it. Upkeep %.2f cr / qtr, paid "
                          "from the treasury.",
                          static_cast<double>(upkeep));
            cells.push_back(construction_cell("bar", bp, construction_grid::bar, buf));
        }

        // Column 7: the press, in the same place as every other row's, DISABLED on
        // a refusal rather than replaced by a wrapped sentence.
        ImGui::TableSetColumnIndex(7);
        const ImVec2 ap = ImGui::GetCursorScreenPos();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4.0f, 0.0f});
        ImGui::BeginDisabled(refusal != nullptr);
        if (ImGui::Button("Build"))
            pressed = true;
        // Published only while the press is on screen: the rows sit in a scrolled
        // list, and a position a click cannot reach is no press. A REFUSED row
        // publishes nothing, which is the absence a check asserts.
        if (refusal == nullptr && ImGui::IsItemVisible())
        {
            px = ap.x + ImGui::GetItemRectSize().x * 0.5f;
            py = ap.y + ImGui::GetItemRectSize().y * 0.5f;
        }
        ImGui::EndDisabled();
        ImGui::PopStyleVar();
        ImGui::PopID();
        {
            char buf[300];
            if (refusal != nullptr)
                std::snprintf(buf, sizeof buf, "%s.", refusal);
            else
                std::snprintf(buf, sizeof buf,
                              "Starts it here and charges the treasury %d cr now.",
                              static_cast<int>(credits));
            cells.push_back(construction_cell("act", ap, construction_grid::act, buf,
                                              refusal == nullptr ? 0u : IM_COL32(230, 140, 140, 255)));
        }

        construction_column_card(name, cells);
        construction_controls::grid_row pub;
        pub.name  = name;
        pub.hover = construction_cells_text(cells);
        pub.x     = gp.x + 24.0f;
        pub.y     = gp.y + line_h * 0.5f;
        pub.h           = row_h;
        pub.name_lines  = name_lines;
        pub.name_elided = name_elided;
        for (const grid_explainer& e : cells)
            pub.cols.push_back({e.key, e.text, (e.mn.x + e.mx.x) * 0.5f, (e.mn.y + e.mx.y) * 0.5f});
        cc.rail_rows.push_back(std::move(pub));
        cc.row_h  = row_h;
        cc.line_h = line_h;
        construction_end_grid();
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
                why = "Can't afford it: no Steel on this market's shelf";
            else if (!std::isfinite(cost) || cost > treasury)
                why = "Can't afford it: the treasury cannot cover the track";
        }
        bool pressed = false;
        grid_row("rail_track##build", "Track", /*rail_glyph=*/true,
                 std::isfinite(cost) ? cost : 0.0f, steel, reg.rail().track_upkeep_per_tile,
                 w.rail_track.count(tile) != 0 ? 1 : 0, 1, why, pressed,
                 cc.rail_track_x, cc.rail_track_y);
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
            why = "Can't afford it: the treasury cannot cover the station";
        int standing = 0;
        for (const auto& [bid, b] : w.buildings)
        {
            (void)bid;
            if (b.type == building_type::railway_station && b.tile == tile)
                ++standing;
        }
        bool pressed = false;
        grid_row("rail_station##build", "Railway Station", /*rail_glyph=*/false,
                 std::isfinite(cost) ? cost : 0.0f, 0.0f,
                 reg.economics(building_type::railway_station).maintenance, standing, 1, why,
                 pressed, cc.rail_station_x, cc.rail_station_y);
        if (pressed)
            ui.construction.pending_station_tile = tile;
    }
    if (which == rail_row::track && !ui.construction.rail_message.empty())
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::neutral), "%s",
                           ui.construction.rail_message.c_str());
}

} // namespace ui
