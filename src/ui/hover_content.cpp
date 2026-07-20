#include "hover_content.hpp"

#include "icons.hpp"
#include "presentation.hpp"
#include "world/components.hpp"
#include "world/logistics.hpp" // road_tier_name / road_traversal_multiplier (BL-184)
#include "world/workforce.hpp"

#include <imgui.h>

namespace ui {

namespace {

// --- Exemplar 1: tile × resource lens -------------------------------------------

void hover_tile_resource(const tile_component& tile, resource_type res)
{
    const resource_presentation& rp = presentation_of(res);

    // Title: composition terrain name.
    ImGui::TextUnformatted(composition_name(tile.composition));

    // Stat line: deposit richness for the selected resource.
    const float dep = tile.resource_deposit[static_cast<std::size_t>(res)];
    ImGui::Text("%s deposit: %.2f", rp.name, static_cast<double>(dep));

    // Why-line: interpret richness bands.
    ImGui::Spacing();
    if (dep <= 0.0f)
        ImGui::TextDisabled("No deposit here");
    else if (dep < 0.25f)
        ImGui::TextDisabled("Trace deposit — low yield");
    else if (dep < 0.75f)
        ImGui::TextDisabled("Moderate deposit");
    else
        ImGui::TextDisabled("Rich deposit — high yield");
}

// Fallback tile content (plain canvas or non-resource lens).
void hover_tile_default(const tile_component& tile)
{
    ImGui::TextUnformatted(composition_name(tile.composition));
    ImGui::Text("Habitability: %.0f%%",
                static_cast<double>(tile.habitability) * 100.0);
}

// --- BL-069: tile × population lens — quick workforce tell ----------------------
// Under the Population lens the hover gives a glance read of the siting signal:
// habitability and the workforce cap it implies. The cap reads the same shared
// curve the simulation applies (workforce.hpp), so the glance cannot drift from
// the lens tint or the sim.
void hover_tile_population(const world& w, const tile_component& tile)
{
    ImGui::TextUnformatted(composition_name(tile.composition));
    ImGui::Text("Habitability: %.0f%%",
                static_cast<double>(tile.habitability) * 100.0);

    const float eff = workforce_efficiency(tile.habitability);
    const float cap = w.workforce_supply(w.player_entity, tile.body) * eff;
    ImGui::Text("Workforce cap: %.1f", static_cast<double>(cap));

    // Why-line: the 0.6 efficiency cliff is the whole point of the lens.
    ImGui::Spacing();
    if (tile.habitability >= 0.6f)
        ImGui::TextDisabled("Full labour \xe2\x80\x94 habitability \xe2\x89\xa5 0.6");
    else
        ImGui::TextDisabled("Reduced labour \xe2\x80\x94 below the 0.6 cliff");
}

// --- Exemplar 2: building × supply lens (or any lens) ---------------------------

void hover_building_supply(const world& w, const building_component& b)
{
    // Title: building type name.
    ImGui::TextUnformatted(building_type_name(b.type));

    // Stat line 1: what it produces / targets.
    if (b.type == building_type::extraction_site)
        ImGui::Text("Extracting: %s", resource_name(b.target_resource));
    else if (b.type == building_type::processing_facility)
        ImGui::TextUnformatted("Processing facility");
    else
        ImGui::TextUnformatted(building_type_name(b.type));

    // Stat line 2: workforce (proxy for output rate; recipe registry not available here).
    ImGui::Text("Workforce: %.0f%%",
                static_cast<double>(b.workforce_assigned) * 100.0);

    // Why-line: explain operational status.
    ImGui::Spacing();
    if (b.decommissioned)
    {
        ImGui::TextDisabled("Decommissioned — no output");
    }
    else if (b.workforce_assigned < 0.1f)
    {
        // Tile deposit check for extraction sites: no deposit = no input material.
        if (b.type == building_type::extraction_site)
        {
            const auto tile_it = w.tiles.find(b.tile);
            if (tile_it != w.tiles.end())
            {
                const float dep = tile_it->second
                    .resource_deposit[static_cast<std::size_t>(b.target_resource)];
                if (dep <= 0.0f)
                    ImGui::TextDisabled("Idle \xe2\x80\x94 no deposit on this tile");
                else
                    ImGui::TextDisabled("Idle \xe2\x80\x94 labour short");
            }
            else
            {
                ImGui::TextDisabled("Idle \xe2\x80\x94 labour short");
            }
        }
        else
        {
            ImGui::TextDisabled("Idle \xe2\x80\x94 no input or labour");
        }
    }
    else if (b.workforce_assigned < 0.5f)
    {
        ImGui::TextDisabled("Understaffed \xe2\x80\x94 reduced output");
    }
    else
    {
        ImGui::TextDisabled("Active");
    }
}

// --- BL-068: rival building hover — type + owner only ---------------------------
// The competitor information-asymmetry rule: a rival's marker, building type, and
// owning corporation are public; production rate and stockpile are private. The
// hover card therefore shows the two public facts and nothing about throughput.
void hover_building_rival(const world& w, entity_id eid, const building_component& b)
{
    // Title: building type (public).
    ImGui::TextUnformatted(building_type_name(b.type));

    // Owner corporation (public) — the only other channel the card may reveal.
    // A tiny inline corp emblem (BL-090) precedes the name so the rival's identity
    // reads the same shape/colour here as on the canvas marker and in the Selection
    // header. Shape + colour route through the shared palette source of truth.
    const entity_id owner = owner_corp_of(w, eid);
    const auto      cit   = w.corporations.find(owner);
    if (cit != w.corporations.end())
    {
        const float  gr = ImGui::GetTextLineHeight() * 0.42f;
        const ImVec2 gc = ImGui::GetCursorScreenPos();
        icons::corp_emblem(ImGui::GetWindowDrawList(),
                           {gc.x + gr, gc.y + ImGui::GetTextLineHeight() * 0.5f}, gr,
                           palette::corp_emblem_shape(owner),
                           palette::corp_identity_colour(owner, w.player_entity));
        ImGui::Dummy({gr * 2.0f + ImGui::GetStyle().ItemSpacing.x, ImGui::GetTextLineHeight()});
        ImGui::SameLine();
        ImGui::Text("Owner: %s", cit->second.name.c_str());
    }
    else
        ImGui::TextDisabled("Owner: unknown");

    // Why-line: name the asymmetry rather than leak production / stockpile.
    ImGui::Spacing();
    ImGui::TextDisabled("Competitor \xe2\x80\x94 output private");
}

// --- Exemplar 3: market-centre × market lens ------------------------------------

void hover_market_market(const world& w, entity_id eid, resource_type res)
{
    const auto mkt_it = w.markets.find(eid);
    if (mkt_it == w.markets.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    const market_component& mkt = mkt_it->second;
    const resource_presentation& rp = presentation_of(res);

    // Title: body market name.
    const auto body_it = w.bodies.find(mkt.body);
    if (body_it != w.bodies.end())
        ImGui::Text("%s Market", body_it->second.name.c_str());
    else
        ImGui::TextUnformatted("Market");

    const std::size_t ri = static_cast<std::size_t>(res);

    // Stat line 1: current price vs base price.
    const float price      = mkt.price[ri];
    const float base_price = mkt.base_price[ri];
    const float move       = price - base_price;
    ImGui::Text("%s price: %.2f", rp.name, static_cast<double>(price));

    // Stat line 2: supply and demand.
    ImGui::Text("Supply %.1f  Demand %.1f",
                static_cast<double>(mkt.supply[ri]),
                static_cast<double>(mkt.demand[ri]));

    // Why-line: interpret the price signal.
    ImGui::Spacing();
    const float supply = mkt.supply[ri];
    const float demand = mkt.demand[ri];
    if (supply <= 0.0f && demand <= 0.0f)
    {
        ImGui::TextDisabled("No activity for this resource");
    }
    else if (move > base_price * 0.1f)
    {
        ImGui::TextDisabled("Price high \xe2\x80\x94 demand outpaces supply");
    }
    else if (move < -base_price * 0.1f)
    {
        ImGui::TextDisabled("Price low \xe2\x80\x94 supply exceeds demand");
    }
    else
    {
        ImGui::TextDisabled("Price stable");
    }
}

// Fallback market content (non-market lens).
void hover_market_default(const world& w, entity_id eid)
{
    const auto mkt_it = w.markets.find(eid);
    if (mkt_it == w.markets.end())
    {
        ImGui::TextDisabled("\xe2\x80\x94");
        return;
    }
    const market_component& mkt = mkt_it->second;
    const auto body_it = w.bodies.find(mkt.body);
    if (body_it != w.bodies.end())
        ImGui::Text("%s Market", body_it->second.name.c_str());
    else
        ImGui::TextUnformatted("Market");

    ImGui::TextDisabled("Switch to Market lens for prices");
}

} // namespace

// --- Public entry point ---------------------------------------------------------

void draw_hover_content(const world& w, const ui_state& ui, entity_id eid)
{
    if (eid == null_entity)
        return;

    // Dispatch: tile
    if (const auto tile_it = w.tiles.find(eid); tile_it != w.tiles.end())
    {
        const tile_component& tile = tile_it->second;
        if (ui.overlay == overlay_mode::resource)
            hover_tile_resource(tile, ui.lens_resource);
        else if (ui.overlay == overlay_mode::population)
            hover_tile_population(w, tile);
        else
            hover_tile_default(tile);

        // BL-184: contextual road-tier read — name the tier + its traversal discount
        // whenever the hovered tile carries one, regardless of active lens.
        if (tile.road_level > 0)
        {
            ImGui::Spacing();
            const float mult = road_traversal_multiplier(tile.road_level);
            ImGui::Text("%s \xe2\x80\x94 %.0f%% traversal cost",
                        road_tier_name(tile.road_level), static_cast<double>(mult) * 100.0);
        }
        return;
    }

    // Dispatch: building. Player-owned buildings show full operational detail;
    // rivals show type + owner only (BL-068 visibility rule).
    if (const auto bld_it = w.buildings.find(eid); bld_it != w.buildings.end())
    {
        if (is_player_owned(w, eid))
            hover_building_supply(w, bld_it->second);
        else
            hover_building_rival(w, eid, bld_it->second);
        return;
    }

    // Dispatch: market
    if (w.markets.count(eid))
    {
        if (ui.overlay == overlay_mode::market)
            hover_market_market(w, eid, ui.lens_resource);
        else
            hover_market_default(w, eid);
        return;
    }

    // Fallback: unknown entity kind — show nothing rather than crashing.
    ImGui::TextDisabled("Entity %u", static_cast<unsigned>(eid));
}

} // namespace ui
