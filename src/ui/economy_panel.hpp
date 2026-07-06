#pragma once

#include "plot_history.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <vector>

struct ui_state; // forward-declared; the full definition lives in ui_state.hpp.

namespace ui {

/// Player economy trend data passed into the economy panel for graph rendering.
/// All series are oldest→newest, capped at plot_history_cap ticks.
struct player_plot_history
{
    const std::vector<float>& balance;      ///< Running balance after each tick.
    const std::vector<float>& income;       ///< Market sales income each tick.
    const std::vector<float>& expenditure;  ///< Auto-buys + wages + maintenance each tick.
};

/// Draw the Layer 3 economy observability panel — the read-only surface that makes
/// the economy legible (see ROADMAP.md). Surfaces, per the Layer 3 design:
///   - each corporation's running balance (negative flagged red);
///   - each (corporation, body) pool's per-resource quantities;
///   - each building's current output rate and idle/active state, and for
///     processors the limiting input;
///   - each body market's supply / demand figures;
///   - player balance / income / expenditure trend plots (BL-063).
///
/// Nothing here is edited — workforce and recipe are authored elsewhere. Read-only.
///
/// The window carries a close button; clicking it clears *p_open so the window
/// closes rather than collapses. When *p_open is false the function draws nothing.
///
/// @param w       Read-only world (pools, markets, corporations).
/// @param reg     Loaded registry (recipe names for processor rows).
/// @param report  The most recent economy step report (building states).
/// @param history Player balance / income / expenditure series for trend graphs.
/// @param ui      UI state — the one-question view selector (economy_view, BL-117).
/// @param p_open  Open/closed flag; gates whether the panel draws.
void draw_economy_panel(const world& w,
                        const recipe_registry& reg,
                        const economy_report& report,
                        const player_plot_history& history,
                        ui_state& ui,
                        bool* p_open);

} // namespace ui
