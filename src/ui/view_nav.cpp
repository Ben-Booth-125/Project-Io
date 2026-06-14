#include "view_nav.hpp"

namespace ui {

namespace {

// Default framings applied on a focus. Pan resets to the canvas centre; zoom
// resets to each rung's natural framing so a jump lands predictably. The
// planetary value matches ui_state's load default (see ui_state.hpp).
constexpr float solar_default_zoom     = 1.0f;        // auto-fit whole system
constexpr float circum_default_zoom    = 1.0f;        // auto-fit local view
constexpr float planetary_default_zoom = 4.0f / 3.0f; // 3/4 of the grid height

} // namespace

void focus_on_body(const world& w, ui_state& ui, entity_id body)
{
    auto it = w.bodies.find(body);
    if (it == w.bodies.end())
        return;

    ui.active_body = body;

    if (it->second.type == body_type::star)
    {
        // The star sits at the system centre, so the Solar view frames it with
        // no pan.
        ui.primary_level = canvas_level::solar;
        ui.solar_pan_x   = 0.0f;
        ui.solar_pan_y   = 0.0f;
        ui.solar_zoom    = solar_default_zoom;
    }
    else
    {
        // Planet/asteroid/station anchor their own Circumplanetary view; a moon
        // anchors its parent's (circumplanetary_anchor resolves this from
        // active_body), with the moon left selected.
        ui.primary_level = canvas_level::circumplanetary;
        ui.circum_pan_x  = 0.0f;
        ui.circum_pan_y  = 0.0f;
        ui.circum_zoom   = circum_default_zoom;
    }
}

void focus_on_surface(const world& w, ui_state& ui, entity_id body)
{
    if (w.bodies.find(body) == w.bodies.end())
        return;

    ui.active_body     = body;
    ui.primary_level   = canvas_level::planetary;
    ui.planetary_pan_x = 0.0f;
    ui.planetary_pan_y = 0.0f;
    ui.planetary_zoom  = planetary_default_zoom;
}

void focus_on_tile(const world& w, ui_state& ui, entity_id tile)
{
    auto it = w.tiles.find(tile);
    if (it == w.tiles.end())
        return;

    focus_on_surface(w, ui, it->second.body);
    ui.active_tile = tile;
}

void focus_on_entity(const world& w, ui_state& ui, entity_id entity)
{
    if (entity == null_entity)
        return;

    if (w.tiles.count(entity))
    {
        // A tile is selected from the surface it lives on, so 'go to' has nothing
        // to descend to — the surface is already framed. Pan-to-tile is out of
        // scope for now, so this is a deliberate no-op rather than a reframe.
        return;
    }
    if (auto b = w.buildings.find(entity); b != w.buildings.end())
    {
        focus_on_tile(w, ui, b->second.tile);
        return;
    }
    if (auto u = w.units.find(entity); u != w.units.end())
    {
        focus_on_surface(w, ui, u->second.body);
        return;
    }
    if (auto m = w.markets.find(entity); m != w.markets.end())
    {
        focus_on_surface(w, ui, m->second.body);
        return;
    }
    if (w.bodies.count(entity))
        focus_on_surface(w, ui, entity);
}

} // namespace ui
