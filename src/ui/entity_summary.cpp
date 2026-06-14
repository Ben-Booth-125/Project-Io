#include "entity_summary.hpp"

#include "presentation.hpp"

#include "format.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace ui {

namespace {

/// Draw the shared resource identity swatch followed by the resource's name on
/// the same line — the pip idiom used by the Tile Ledger market table and the
/// surface-canvas tooltip, factored here so every summary reads identically.
///
/// @param rp Presentation metadata for the resource (name + identity colour).
void draw_resource_swatch(const resource_presentation& rp)
{
    const float  sw  = ImGui::GetTextLineHeight();
    const ImVec2 swp = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(swp, {swp.x + sw, swp.y + sw}, rp.colour);
    ImGui::Dummy({sw, sw});
    ImGui::SameLine();
}

/// Draw a solid colour swatch followed by SameLine — the same pip idiom as
/// draw_resource_swatch but taking an arbitrary colour, so a nation's identity
/// colour can prefix its name the way a resource colour prefixes a deposit.
///
/// @param colour The swatch fill colour.
void draw_colour_swatch(ImU32 colour)
{
    const float  sw  = ImGui::GetTextLineHeight();
    const ImVec2 swp = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(swp, {swp.x + sw, swp.y + sw}, colour);
    ImGui::Dummy({sw, sw});
    ImGui::SameLine();
}

/// Display name for a nation's political ideology.
///
/// @param v Ideology to name.
/// @return  Null-terminated display name.
const char* ideology_name(::ideology v)
{
    switch (v)
    {
    case ::ideology::authoritarian: return "Authoritarian";
    case ::ideology::technocratic:  return "Technocratic";
    case ::ideology::mercantile:    return "Mercantile";
    case ::ideology::isolationist:  return "Isolationist";
    }
    return "Unknown";
}

/// Display name for a nation's territorial posture.
///
/// @param v Expansionism to name.
/// @return  Null-terminated display name.
const char* expansionism_name(::expansionism v)
{
    switch (v)
    {
    case ::expansionism::passive:    return "Passive";
    case ::expansionism::moderate:   return "Moderate";
    case ::expansionism::aggressive: return "Aggressive";
    }
    return "Unknown";
}

/// Display name for a nation's dominant economic activity.
///
/// @param v Economic focus to name.
/// @return  Null-terminated display name.
const char* economic_focus_name(::economic_focus v)
{
    switch (v)
    {
    case ::economic_focus::extraction: return "Extraction";
    case ::economic_focus::processing: return "Processing";
    case ::economic_focus::trade:      return "Trade";
    }
    return "Unknown";
}

/// Display name for a corporation's primary industrial focus.
///
/// @param v Industrial focus to name.
/// @return  Null-terminated display name.
const char* industrial_focus_name(::industrial_focus v)
{
    switch (v)
    {
    case ::industrial_focus::extraction: return "Extraction";
    case ::industrial_focus::processing: return "Processing";
    case ::industrial_focus::trade:      return "Trade";
    }
    return "Unknown";
}

} // namespace

void draw_body_summary(const world& w, entity_id id)
{
    auto it = w.bodies.find(id);
    if (it == w.bodies.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94"); // em-dash: stale/missing selection
        return;
    }

    const body_component& b = it->second;

    // Title line: name in the selection colour, then the body type.
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", b.name.c_str());
    ImGui::TextDisabled("%s", body_type_name(b.type));

    ImGui::Text("Orbit: %.2f AU", b.orbital_radius_au);

    // Parent body, when this one orbits another (a moon around its planet).
    if (b.parent != null_entity)
    {
        auto parent_it = w.bodies.find(b.parent);
        if (parent_it != w.bodies.end())
            ImGui::Text("Orbits: %s", parent_it->second.name.c_str());
    }

    ImGui::Text("Grid: %dx%d tiles", b.grid_width, b.grid_height);
}

void draw_tile_summary(const world& w, entity_id id)
{
    auto it = w.tiles.find(id);
    if (it == w.tiles.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }

    const tile_component& tile = it->second;

    // Coordinate + terrain header, mirroring the surface-canvas hover tooltip.
    ImGui::Text("[%d, %d]", tile.grid_x, tile.grid_y);
    ImGui::Text("%s \xc2\xb7 %s", composition_name(tile.composition),
                                  landform_name(tile.landform));
    ImGui::Text("Hazard: %.2f", tile.hazard_level);
    ImGui::Text("Habitability: %.2f", tile.habitability);

    // Non-zero deposits, each with its identity swatch.
    bool any_deposit = false;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (tile.resource_deposit[r] > 0.0f)
        {
            const resource_presentation& rp = presentation_of(static_cast<resource_type>(r));
            draw_resource_swatch(rp);
            ImGui::Text("%s: %.1f", rp.name, tile.resource_deposit[r]);
            any_deposit = true;
        }
    }
    if (!any_deposit)
        ImGui::TextDisabled("No deposits");
}

void draw_building_summary(const world& w, entity_id id)
{
    auto it = w.buildings.find(id);
    if (it == w.buildings.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }

    const building_component& b = it->second;

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                       "%s", building_type_name(b.type));

    // Host tile coordinates, resolved through the building's tile reference.
    auto tile_it = w.tiles.find(b.tile);
    if (tile_it != w.tiles.end())
    {
        const tile_component& t = tile_it->second;
        ImGui::Text("Tile: [%d, %d]", t.grid_x, t.grid_y);
    }

    ImGui::Text("Workforce: %.0f%%", b.workforce_assigned * 100.0f);
}

void draw_market_summary(const world& w, entity_id id)
{
    auto it = w.markets.find(id);
    if (it == w.markets.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }

    const market_component& mkt = it->second;

    // Host body name, when resolvable.
    auto body_it = w.bodies.find(mkt.body);
    if (body_it != w.bodies.end())
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                           "%s Market", body_it->second.name.c_str());
    else
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Market");

    // Compact per-resource supply / demand / price table — a trimmed version of
    // the full Tile Ledger market table (base price omitted to keep it readable
    // as a summary). Only resources with any supply or demand are listed.
    if (ImGui::BeginTable("market_summary", 4,
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_RowBg        | ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("Resource", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Supply",   ImGuiTableColumnFlags_WidthFixed,  70.0f);
        ImGui::TableSetupColumn("Demand",   ImGuiTableColumnFlags_WidthFixed,  70.0f);
        ImGui::TableSetupColumn("Price",    ImGuiTableColumnFlags_WidthFixed,  60.0f);
        ImGui::TableHeadersRow();

        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (mkt.supply[r] <= 0.0f && mkt.demand[r] <= 0.0f)
                continue;

            const resource_presentation& rp = presentation_of(static_cast<resource_type>(r));
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            draw_resource_swatch(rp);
            ImGui::TextUnformatted(rp.name);

            ImGui::TableSetColumnIndex(1); ImGui::Text("%.1f", mkt.supply[r]);
            ImGui::TableSetColumnIndex(2); ImGui::Text("%.1f", mkt.demand[r]);

            // Price coloured by its move against the base price, as in the ledger.
            ImGui::TableSetColumnIndex(3);
            const float move = mkt.price[r] - mkt.base_price[r];
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(value_colour(move)),
                               "%.2f", mkt.price[r]);
        }
        ImGui::EndTable();
    }
}

void draw_unit_summary(const world& w, entity_id id)
{
    auto it = w.units.find(id);
    if (it == w.units.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }

    const unit_component& u = it->second;

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                       "Unit group");
    ImGui::Text("Count: %d", u.count);

    // Location: name the body the unit currently occupies, when resolvable.
    auto body_it = w.bodies.find(u.body);
    if (body_it != w.bodies.end())
        ImGui::Text("Location: %s", body_it->second.name.c_str());
}

void draw_nation_summary(const world& w, entity_id id)
{
    auto it = w.nations.find(id);
    if (it == w.nations.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }

    const nation_component& n = it->second;

    // Title line: identity swatch, then the nation name in the selection colour.
    draw_colour_swatch(palette::nation_colour(id));
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", n.name.c_str());

    ImGui::Text("Tiles: %zu", n.tiles.size());
    ImGui::Text("Ideology: %s", ideology_name(n.ideology));
    ImGui::Text("Expansionism: %s", expansionism_name(n.expansionism));
    ImGui::Text("Economic focus: %s", economic_focus_name(n.economic_focus));

    // Top resources by abundance: collect non-zero entries, sort descending,
    // and show the leading three with their identity swatches.
    std::vector<std::pair<std::size_t, float>> ranked;
    for (std::size_t r = 0; r < resource_count; ++r)
        if (n.resource_abundance[r] > 0.0f)
            ranked.emplace_back(r, n.resource_abundance[r]);

    if (ranked.empty())
    {
        ImGui::TextDisabled("No notable resources");
    }
    else
    {
        const std::size_t top = std::min<std::size_t>(3, ranked.size());
        std::partial_sort(ranked.begin(), ranked.begin() + top, ranked.end(),
                          [](const auto& a, const auto& b) { return a.second > b.second; });
        for (std::size_t i = 0; i < top; ++i)
        {
            const resource_presentation& rp =
                presentation_of(static_cast<resource_type>(ranked[i].first));
            draw_resource_swatch(rp);
            ImGui::Text("%s: %.0f", rp.name, ranked[i].second);
        }
    }
}

void draw_corporation_summary(const world& w, entity_id id)
{
    auto it = w.corporations.find(id);
    if (it == w.corporations.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }

    const corporation_component& c = it->second;

    // Title line: name in the selection colour, flagged when it is the player's.
    const bool is_player = c.is_player || id == w.player_entity;
    if (is_player)
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                           "%s (Player)", c.name.c_str());
    else
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                           "%s", c.name.c_str());

    // Home nation, when the registration reference resolves.
    auto nation_it = w.nations.find(c.home_nation);
    if (nation_it != w.nations.end())
        ImGui::Text("Nation: %s", nation_it->second.name.c_str());

    ImGui::Text("Focus: %s", industrial_focus_name(c.focus));
    ImGui::Text("Capital: %s", ui::fmt::credits(c.starting_capital).c_str());
    ImGui::Text("Assets: %zu", c.assets.size());
}

} // namespace ui
