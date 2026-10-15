#pragma once
// BL-1255 (rail construction UI): the rail rows of the Construction view's
// per-tile building list. Rail is STATE INDUSTRY (NATIONS.md § State
// industry; LOGISTICS.md § 4c): track and stations are laid for the seat's
// NATION and paid from its treasury, never the corporation's balance.
//
// Everything here reads a const world: the legality it reports is a PREVIEW of
// the door's own rules (construction.cpp lay_track / construct_for), so a tile
// the preview accepts is the tile the seam will be asked to lay. The seam stays
// the authority — app::render issues lay_track / build type 11 through
// apply_corp_command and words whatever it refuses.

#include "ui_state.hpp"
#include "world/world.hpp"

#include <vector>

class recipe_registry;

namespace ui
{

/// The seat's home nation when it is a MAJOR (has a champion), else null_entity.
entity_id rail_player_nation(const world& w);

/// Whether @p tile may be appended to the planned @p run of track for @p nation:
/// land, the nation's own ground, carrying no track and not already planned, and
/// either opening/extending a line (a city tile, or a cardinal neighbour of the
/// nation's track) when the run is empty, or adjacent to the run's last tile.
/// @p why (optional) receives a short reason on refusal.
bool rail_track_tile_ok(const world& w, entity_id nation, entity_id tile,
                        const std::vector<entity_id>& run, const char** why = nullptr);

/// Whether a station may stand on @p tile for @p nation: its own ground, a city,
/// track on it or on a cardinal neighbour, and no station there yet.
bool rail_station_tile_ok(const world& w, entity_id nation, entity_id tile,
                          const char** why = nullptr);

/// Whether @p a and @p b are cardinal neighbours on the same body (columns wrap).
bool rail_tiles_adjacent(const world& w, entity_id a, entity_id b);

/// The rail rows of the Construction view's per-tile building list (LOGISTICS.md
/// 4c: rail is built like anything else). Track and Railway Station each draw as a
/// building row for @p tile - cost (credits + steel), upkeep, and Build or the
/// refusal. A Build press queues `pending_track_tile` / `pending_station_tile` for
/// app::render. Publishes the Build presses into `ui.construction_ui`
/// (`rail_track_*`, `rail_station_*`; -1 when the row is refused or absent).
/// @p which picks the row: Track draws after the road ladder, Railway Station
/// inside the Infrastructure fold (Ben, 2026-10-15).
enum class rail_row { track, station };
void draw_rail_rows(const world& w, const recipe_registry& reg, ui_state& ui, entity_id tile,
                    float row_h, rail_row which);

} // namespace ui
