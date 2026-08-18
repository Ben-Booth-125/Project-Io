#include "canvas_command.hpp"

#include "view_nav.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace ui {

namespace {

// One keypress / command step. Pan is in screen pixels (matched to the canvas's
// own pan units); zoom is a multiplicative factor mirroring the scroll-wheel step
// in the canvases (1.1x per notch).
constexpr float pan_step  = 80.0f;
constexpr float zoom_step = 1.1f;

// Number of overlay_mode values, for wrap-around lens cycling. Derived from the
// enum's `count` sentinel, so a new lens is reachable without touching a literal
// here: a hand-kept count went stale twice (10 when industry landed, 13 when
// BL-226's continent insert left supply_routes beyond the cycle).
constexpr int overlay_mode_count = static_cast<int>(overlay_mode::count);

/// Pointers to the pan/zoom fields of whichever rung is currently primary, so the
/// pan/zoom commands act on the foregrounded canvas without per-rung branches.
struct rung_view
{
    float* pan_x;
    float* pan_y;
    float* zoom;
};

rung_view current_rung(ui_state& ui)
{
    switch (ui.primary_level)
    {
        case canvas_level::solar:
            return {&ui.solar_pan_x, &ui.solar_pan_y, &ui.solar_zoom};
        case canvas_level::circumplanetary:
            return {&ui.circum_pan_x, &ui.circum_pan_y, &ui.circum_zoom};
        case canvas_level::planetary:
            return {&ui.planetary_pan_x, &ui.planetary_pan_y, &ui.planetary_zoom};
    }
    return {&ui.solar_pan_x, &ui.solar_pan_y, &ui.solar_zoom};
}

/// Anchor the body `step` places (by ascending id) from the current anchor and
/// re-frame it at the current rung. Wraps around the body list. A world with no
/// bodies is a no-op.
void cycle_body(const world& w, ui_state& ui, int step)
{
    if (w.bodies.empty())
        return;

    std::vector<entity_id> ids;
    ids.reserve(w.bodies.size());
    for (const auto& [id, _] : w.bodies)
        ids.push_back(id);
    std::sort(ids.begin(), ids.end());

    auto it  = std::find(ids.begin(), ids.end(), ui.active_body);
    int  idx = (it == ids.end()) ? 0 : static_cast<int>(it - ids.begin());
    const int n = static_cast<int>(ids.size());
    idx = ((idx + step) % n + n) % n;

    const entity_id target = ids[idx];
    if (ui.primary_level == canvas_level::planetary)
        focus_on_surface(w, ui, target);
    else
        focus_on_body(w, ui, target);
}

} // namespace

void apply_canvas_command(const world& w, ui_state& ui, canvas_command cmd)
{
    switch (cmd)
    {
        case canvas_command::descend:
        {
            // Selection-aware descend (BL-165): if a surface-bearing entity is
            // selected — a body, or a building/unit/market that lives on one —
            // dive into it via focus_on_entity, re-anchoring to the selection and
            // landing on its most informative rung (identical to double-click /
            // the panel's 'go to'). So single-click a body, then Enter, dives into
            // *that* body rather than dropping a rung on the previous anchor.
            // Tiles/nations/corporations are excluded (a tile 'go to' is a no-op;
            // corp/nation would open a ledger, not descend) and fall through to the
            // blind one-rung drop below, which also serves the no-selection case.
            const entity_id sel = ui.selected_entity;
            if (sel != null_entity &&
                (w.bodies.count(sel) || w.buildings.count(sel) ||
                 w.units.count(sel)  || w.markets.count(sel)))
            {
                focus_on_entity(w, ui, sel);
                break;
            }
            if (ui.primary_level == canvas_level::solar)
                ui.primary_level = canvas_level::circumplanetary;
            else if (ui.primary_level == canvas_level::circumplanetary)
                ui.primary_level = canvas_level::planetary;
            break;
        }

        case canvas_command::ascend:
            if (ui.primary_level == canvas_level::planetary)
                ui.primary_level = canvas_level::circumplanetary;
            else if (ui.primary_level == canvas_level::circumplanetary)
                ui.primary_level = canvas_level::solar;
            break;

        case canvas_command::body_next: cycle_body(w, ui, +1); break;
        case canvas_command::body_prev: cycle_body(w, ui, -1); break;

        case canvas_command::pan_left:  *current_rung(ui).pan_x += pan_step; break;
        case canvas_command::pan_right: *current_rung(ui).pan_x -= pan_step; break;
        case canvas_command::pan_up:    *current_rung(ui).pan_y += pan_step; break;
        case canvas_command::pan_down:  *current_rung(ui).pan_y -= pan_step; break;

        case canvas_command::zoom_in:  *current_rung(ui).zoom *= zoom_step; break;
        case canvas_command::zoom_out: *current_rung(ui).zoom /= zoom_step; break;

        case canvas_command::lens_next:
            ui.overlay = static_cast<overlay_mode>(
                (static_cast<int>(ui.overlay) + 1) % overlay_mode_count);
            break;
        case canvas_command::lens_prev:
            ui.overlay = static_cast<overlay_mode>(
                (static_cast<int>(ui.overlay) + overlay_mode_count - 1) % overlay_mode_count);
            break;
        case canvas_command::lens_clear:
            ui.overlay = overlay_mode::none;
            break;

        // These require sim_loop / app state; dispatched by app::dispatch_action.
        case canvas_command::pause_toggle:
        case canvas_command::speed_1:
        case canvas_command::speed_2:
        case canvas_command::speed_3:
        case canvas_command::speed_4:
        case canvas_command::speed_5:
        case canvas_command::help_toggle:
        case canvas_command::options_toggle:
        case canvas_command::tech_tree_toggle:
            break;
    }
}

std::optional<canvas_command> canvas_command_from_name(std::string_view name)
{
    // Pairs the command vocabulary (verify scripts + keybinding table) to the enum.
    static const std::array<std::pair<std::string_view, canvas_command>, 22> table = {{
        {"descend",       canvas_command::descend},
        {"ascend",        canvas_command::ascend},
        {"body_next",     canvas_command::body_next},
        {"body_prev",     canvas_command::body_prev},
        {"pan_left",      canvas_command::pan_left},
        {"pan_right",     canvas_command::pan_right},
        {"pan_up",        canvas_command::pan_up},
        {"pan_down",      canvas_command::pan_down},
        {"zoom_in",       canvas_command::zoom_in},
        {"zoom_out",      canvas_command::zoom_out},
        {"lens_next",     canvas_command::lens_next},
        {"lens_prev",     canvas_command::lens_prev},
        {"lens_clear",    canvas_command::lens_clear},
        {"pause_toggle",  canvas_command::pause_toggle},
        {"speed_1",       canvas_command::speed_1},
        {"speed_2",       canvas_command::speed_2},
        {"speed_3",       canvas_command::speed_3},
        {"speed_4",       canvas_command::speed_4},
        {"speed_5",       canvas_command::speed_5},
        {"help_toggle",   canvas_command::help_toggle},
        {"options_toggle", canvas_command::options_toggle},
        {"tech_tree_toggle", canvas_command::tech_tree_toggle},
    }};

    for (const auto& [n, cmd] : table)
        if (n == name)
            return cmd;
    return std::nullopt;
}

} // namespace ui
