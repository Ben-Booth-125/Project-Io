#pragma once

#include "world/world.hpp"

#include <vector>

struct economy_report;
class recipe_registry;

namespace ui {

/// Draw the player's persistent financial header strip.
///
/// A full-width strip across the top of the canvas area, between the profile
/// (top-left) and the time column (top-right). It is the player's glanceable
/// financial dashboard — always visible, never opened or closed. It surfaces the
/// three figures the player needs without opening a ledger (see docs/ui/HEADER.md):
///   - the player NATION's **treasury** (BL-1406; a nation never runs into debt);
///   - the **cargo** valuation — the player's trades' goods in transit, at the
///     price where they land;
///   - the **last-tick net** change (the nation's state-ledger row) as a coloured
///     per-quarter figure plus a sparkline of recent treasuries, and the runway;
///   - at the top right, the nation's three **recruit pools** - land, air, sea -
///     each a glyph, held / cap and a thin bar (BL-1472; MILITARY.md § Recruits).
///
/// Detail lives in the ledgers; this stays a summary surface.
///
/// @param w               Read-only world (player corporation, pools, markets).
/// @param reg             The registry: the recruit rates a pool's cap and output read.
/// @param balance_history Recent player balances, oldest→newest, one per economy
///                        tick; drives the net figure and the sparkline.
/// @param report          This tick's economy report — read only for
///                        `budgets[player].subsidies` (BL-577), so the RUNWAY
///                        tooltip can name a contract payment that moved the
///                        balance rather than leaving it as an unexplained jump.
/// @param left            Left edge of the strip (clear of the navigation pane).
/// @param right           Right edge of the strip (clear of the time column).
void draw_header_panel(const world& w,
                       const recipe_registry& reg,
                       const std::vector<float>& balance_history,
                       const economy_report& report,
                       float left,
                       float right);

/// Estimated value of the goods the player OWNS — its trades' cargo in transit
/// (BL-1269; no corporation holds a stockpile under the shelf economy), each
/// in-flight convoy's cargo priced at its destination market's current price. The
/// header strip's "CARGO" figure; also the Budget ledger's "Cargo Value" (BL-171).
/// Exported so both surfaces share one valuation rather than duplicating it.
float player_cargo_value(const world& w);

/// BL-1472: what the header's recruit readouts drew this frame - the verify record,
/// so a script checks the numbers against the world and hovers the real readout.
struct header_recruit_drawn
{
    int   held = 0, cap = 0, output = 0; ///< As read for the readout (its tooltip carries all three).
    float x = -1.0f, y = -1.0f;          ///< The readout's centre, screen pixels (its hover).
    float bar = 0.0f;                    ///< The bar's fill, held over cap (0..1).
    bool  hovered = false;               ///< Its tooltip is showing.
};
struct header_recruits_record
{
    int frame = -1; ///< ImGui frame it was drawn on (-1 = never).
    /// The fit ladder's step (HEADER.md § Fit and degradation): 0 = "held / cap",
    /// 1 = held alone, 2 = glyph and bar only (the numbers in the tooltip).
    int fit = 0;
    header_recruit_drawn theatre[3]; ///< 0 land, 1 air, 2 sea.
    /// The runway's fit, resolved before the readouts (HEADER.md § Fit and degradation):
    /// -1 not shown because the treasury is not burning, 0 folded into NET's tooltip,
    /// 1 its value alone, 2 label and value.
    int   runway = -1;
    float room = 0.0f;        ///< Width the money run may take: the strip less the readouts' floor.
    float money_used = 0.0f;  ///< Width the treasury, cargo and NET took before the runway.
    float runway_full = 0.0f; ///< Width "   |   RUNWAY  12q" needs (a nominal 12q when not burning).
    float runway_value = 0.0f; ///< Width "   |   12q" needs, the value alone.
};
const header_recruits_record& header_recruits_drawn();

/// Height of the header strip in pixels. Matches the identity card
/// (profile_panel_height, 92) so the balance bar and the identity tile read as one
/// level top band across the window; the strip's content is vertically centred.
inline constexpr float header_panel_height = 92.0f;

} // namespace ui
