#pragma once

#include "world/world.hpp"

#include <vector>

struct economy_report;

namespace ui {

/// Draw the player's persistent financial header strip.
///
/// A full-width strip across the top of the canvas area, between the profile
/// (top-left) and the time column (top-right). It is the player's glanceable
/// financial dashboard — always visible, never opened or closed. It surfaces the
/// three figures the player needs without opening a ledger (see docs/ui/HEADER.md):
///   - the player corporation's running **balance** (negatives flagged red);
///   - an **estimated stockpile valuation** — the player's `(corp, body)` pools
///     summed at each body's market price;
///   - the **last-tick net** change as a coloured per-quarter figure plus a
///     sparkline of recent balances.
///
/// Detail lives in the ledgers; this stays a summary surface.
///
/// @param w               Read-only world (player corporation, pools, markets).
/// @param balance_history Recent player balances, oldest→newest, one per economy
///                        tick; drives the net figure and the sparkline.
/// @param report          This tick's economy report — read only for
///                        `budgets[player].subsidies` (BL-577), so the RUNWAY
///                        tooltip can name a contract payment that moved the
///                        balance rather than leaving it as an unexplained jump.
/// @param left            Left edge of the strip (clear of the navigation pane).
/// @param right           Right edge of the strip (clear of the time column).
void draw_header_panel(const world& w,
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

/// Height of the header strip in pixels. Matches the identity card
/// (profile_panel_height, 92) so the balance bar and the identity tile read as one
/// level top band across the window; the strip's content is vertically centred.
inline constexpr float header_panel_height = 92.0f;

} // namespace ui
