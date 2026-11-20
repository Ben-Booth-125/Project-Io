#include "view_nav.hpp"

#include "market_ledger.hpp" // market_naming_centre: a market's capital

namespace ui {

namespace {

// Default framings applied on a focus. Pan resets to the canvas centre; zoom
// resets to each rung's natural framing so a jump lands predictably. The
// planetary value matches ui_state's load default (see ui_state.hpp).
constexpr float solar_default_zoom     = 1.0f;        // auto-fit whole system
constexpr float circum_default_zoom    = 1.0f;        // auto-fit local view
constexpr float planetary_default_zoom = 4.0f / 3.0f; // 3/4 of the grid height
// A 'go to' lands CLOSE (Ben, 2026-11-20): the second-nearest rung of the stepped
// ladder (body_surface_canvas.cpp, kMaxZoom 17.1 / 2), where a city and its
// buildings read. The default framing is the whole map, and a go-to that zooms out
// to it reads as having gone nowhere.
constexpr float planetary_goto_zoom    = 17.1f / 2.0f;

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

    // Focusing a surface resets the view, so cancel any pending centre-on-tile
    // request — a deliberate navigation supersedes a queued recentre (e.g. the
    // campaign-start HQ framing set in app::setup_world). Without this, a stale
    // pending centre would fire on the newly focused body's first draw.
    ui.planetary_center_pending = false;
}

void focus_on_tile(const world& w, ui_state& ui, entity_id tile)
{
    auto it = w.tiles.find(tile);
    if (it == w.tiles.end())
        return;

    // Keep a view already this close on this body; otherwise land at the go-to rung.
    const bool  same_view = ui.primary_level == canvas_level::planetary
                         && ui.active_body == it->second.body;
    const float keep_zoom = (same_view && ui.planetary_zoom >= planetary_goto_zoom)
                                ? ui.planetary_zoom : planetary_goto_zoom;
    focus_on_surface(w, ui, it->second.body);
    ui.planetary_zoom = keep_zoom;

    // Land ON the tile (Ben's 2026-11-09 playtest): focus_on_surface alone resets the
    // pan to the grid's centre, so a 'go to' framed whatever tile sits there. The
    // canvas applies this on its next draw, where the grid transform is known.
    ui.planetary_center_col     = it->second.grid_x;
    ui.planetary_center_row     = it->second.grid_y;
    ui.planetary_center_pending = true;
}

void focus_on_entity(const world& w, ui_state& ui, entity_id entity)
{
    if (entity == null_entity)
        return;

    // Every spatial kind lands ON its tile (Ben's 2026-11-19 playtest, BL-1693:
    // a market's 'go to' framed the grid's centre instead of its own tile).
    if (w.tiles.count(entity))
    {
        focus_on_tile(w, ui, entity);
        return;
    }
    if (auto b = w.buildings.find(entity); b != w.buildings.end())
    {
        focus_on_tile(w, ui, b->second.tile);
        return;
    }
    if (auto u = w.units.find(entity); u != w.units.end())
    {
        // position is a tile id (BL-157/BL-324): the unit's own tile, centred.
        focus_on_tile(w, ui, u->second.position);
        return;
    }
    if (auto m = w.markets.find(entity); m != w.markets.end())
    {
        // The market's CAPITAL (Ben, 2026-11-20): the population centre it is named
        // for. Its clearing centre_tile is a trade site near it, not the city. A
        // market no centre names falls back to that tile, an unanchored one to its body.
        const entity_id pc = market_naming_centre(w, entity);
        if (auto ct = w.population_centre_tile.find(pc);
            pc != null_entity && ct != w.population_centre_tile.end() && w.tiles.count(ct->second))
            focus_on_tile(w, ui, ct->second);
        else if (m->second.centre_tile != null_entity && w.tiles.count(m->second.centre_tile))
            focus_on_tile(w, ui, m->second.centre_tile);
        else
            focus_on_surface(w, ui, m->second.body);
        return;
    }
    if (w.bodies.count(entity))
    {
        focus_on_surface(w, ui, entity);
        return;
    }
    // A nation goes to its capital (Ben, 2026-11-20).
    if (auto n = w.nations.find(entity); n != w.nations.end())
    {
        if (n->second.capital_tile != null_entity && w.tiles.count(n->second.capital_tile))
            focus_on_tile(w, ui, n->second.capital_tile);
        return;
    }
}

} // namespace ui
