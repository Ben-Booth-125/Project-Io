#pragma once

#include "ui_state.hpp"
#include "world/world.hpp"

#include <optional>
#include <string_view>

namespace ui {

/// A discrete canvas navigation action. This is the shared vocabulary that backs
/// *both* the player's keyboard bindings (app::process_events) and the verify Lua
/// API (verify.command), so a verification script reads as the same command
/// sequence a player would key. The keybinding table lives in
/// docs/ui/CANVASES.md § Keyboard.
///
/// Capture is deliberately *not* a command here: it needs the renderer and is
/// owned by app (F12 / verify.capture). Every command below is a pure ui_state
/// mutation, which is what lets the verify session drive it deterministically.
enum class canvas_command
{
    descend,    ///< Descend one ladder rung (solar → circumplanetary → planetary).
    ascend,     ///< Ascend one ladder rung (planetary → circumplanetary → solar).
    body_next,  ///< Anchor the next body (by id) and re-frame it at the current rung.
    body_prev,  ///< Anchor the previous body (by id) and re-frame it at the current rung.
    pan_left,   ///< Pan the current rung's view left by one step.
    pan_right,  ///< Pan the current rung's view right by one step.
    pan_up,     ///< Pan the current rung's view up by one step.
    pan_down,   ///< Pan the current rung's view down by one step.
    zoom_in,    ///< Zoom the current rung in by one step.
    zoom_out,   ///< Zoom the current rung out by one step.
    lens_next,  ///< Cycle the overlay lens forward (wrapping).
    lens_prev,  ///< Cycle the overlay lens backward (wrapping).
    lens_clear, ///< Clear the overlay lens (overlay_mode::none).
};

/// Apply a canvas command to the shared UI state. Pure state mutation — no
/// rendering, no input polling — so it is identical whether driven by a key press
/// or a verify script. A command that does not apply at the current rung (e.g.
/// descend past the surface) is a no-op.
///
/// @param w   Read-only world state (needed to resolve body cycling and framing).
/// @param ui  UI state to mutate.
/// @param cmd The command to apply.
void apply_canvas_command(const world& w, ui_state& ui, canvas_command cmd);

/// Map a command name (the vocabulary used by verify scripts and the keybinding
/// table — e.g. "descend", "pan_left", "lens_next") to its enum. Unknown names
/// return std::nullopt.
///
/// @param name The command name.
/// @return The matching command, or std::nullopt if the name is unknown.
std::optional<canvas_command> canvas_command_from_name(std::string_view name);

} // namespace ui
