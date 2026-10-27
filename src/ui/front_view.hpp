#pragma once

#include "world/fronts.hpp"

#include <imgui.h>

#include <string>
#include <vector>

struct world;

namespace ui {

// ---------------------------------------------------------------------------
// The presentation side of fronts (BL-1427; MILITARY.md § Fronts,
// SELECTION.md § The front element). Fronts are DERIVED by the world
// (`derive_fronts`); this file only caches that answer for the surfaces that
// draw it and adds what a screen needs: the hex edges each span runs along,
// the label that names a front, and the stance a front is coloured by.
//
// Nothing here derives a front. With no fronts from the world, every surface
// that reads this draws none.
// ---------------------------------------------------------------------------

/// One shared hex edge of a front: span tile (a) and the neighbour's tile (b)
/// across it, in grid coordinates on the span's body.
struct front_edge
{
    int a_col = 0, a_row = 0;
    int b_col = 0, b_row = 0;
};

/// A front as the UI reads it: the world's `front`, its label, its edges.
struct front_view
{
    front                   f;
    entity_id               body = null_entity; ///< The body its span lies on.
    std::vector<front_edge> edges;              ///< Every span-to-neighbour hex side.
    std::string             label;              ///< "Varneth — 14 tiles", disambiguated.
};

/// Every front of @p nation, cached: re-derived only when the world, the nation,
/// the day tick or the territory map's size moves (a border changes on a tick).
/// Ordered as `derive_fronts` orders them.
const std::vector<front_view>& fronts_of(const world& w, entity_id nation);

/// The front of @p nation keyed @p key, or nullptr when no such front exists now.
const front_view* front_by_key(const world& w, entity_id nation, entity_id key);

/// Units of @p nation assigned to the front keyed @p key, in ascending id.
std::vector<entity_id> front_units(const world& w, entity_id nation, entity_id key);

/// The stance of @p nation toward @p neighbour, as a front is coloured by it.
enum class front_stance { hostile, neutral, friendly };
front_stance stance_toward(const world& w, entity_id nation, entity_id neighbour);

/// The stance's line colour (presentation.hpp's palette).
ImU32 front_stance_colour(front_stance s);

/// "Hostile" / "Neutral" / "Friendly".
const char* front_stance_name(front_stance s);

/// Whether @p nation may order its front toward @p neighbour to push: only when
/// it is hostile to it (the verb's own rule, `set_front_order`).
bool front_push_open(const world& w, entity_id nation, entity_id neighbour);

} // namespace ui
