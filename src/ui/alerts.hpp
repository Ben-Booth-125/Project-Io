#pragma once

/// @file alerts.hpp
/// The alert strip (BL-1515; LAYOUT.md § Alert strip): what wants the player's
/// attention, at the top centre of the canvas area, each alert a click through to the
/// surface that answers it. It informs and never acts.

#include <string>
#include <vector>

struct economy_report;
class recipe_registry;
struct ui_state;
struct world;

namespace ui {

enum class alert_kind
{
    research_not_set,
    idle_buildings,
    wars,
    low_runway,
    construction_stalled,
    deposit_exhausted,
    trades_exceed_points,
};

/// One alert as drawn this frame, so a check presses the real control.
struct alert_drawn
{
    alert_kind  kind = alert_kind::research_not_set;
    std::string text;
    float       x = -1.0f, y = -1.0f; ///< The press's centre, screen px.
};

/// The strip's rows, recomputed from the live world and the last economy report. Empty
/// with nobody seated. Pure: a read of what the screen already says elsewhere.
std::vector<alert_drawn> read_alerts(const world& w, const recipe_registry& reg,
                                     const std::vector<float>& balance_history,
                                     const economy_report& report);

/// Draw the strip, centred between @p left and @p right, just below the header. Draws
/// nothing with nothing to say, so it never takes canvas input for no reason.
void draw_alert_strip(const world& w, const recipe_registry& reg,
                      const std::vector<float>& balance_history, const economy_report& report,
                      ui_state& ui, float left, float right);

/// The rows the strip drew on its last frame.
const std::vector<alert_drawn>& alerts_drawn();

} // namespace ui
