#pragma once

#include "ui_state.hpp"
#include "world/hard_coded_world.hpp" // generation_report — the tile-pass inputs to replay
#include "world/tile_generation.hpp"  // generation_record — the per-pass intermediates
#include "world/world.hpp"

namespace ui {

/// Draws the Generation Ledger — the developer tuning surface that explains WHY a
/// tile generated as it did. Design authority: docs/generation/GENERATION_LEDGER.md.
///
/// One flat panel of stacked per-body sections — composition/landform histograms,
/// the ocean threshold against the profile's target, the latitude bands, and the
/// profile that drove all of it. There is no per-tile view: the old Tile view and
/// its breadcrumb builder were retired (Ben, 2026-08-30) and the design left
/// GENERATION_LEDGER.md with them (Ben, 2026-09-15). A tile's "why", if it returns,
/// is a Selection subject, not a ledger view.
///
/// The per-pass intermediates it reads are REGENERATED ON DEMAND from the report's
/// recorded tile-pass inputs and cached for as long as one body stays the subject;
/// they are never stored on the world and never reach the save. Generation is
/// deterministic in those inputs, so a stored copy would be bloat, not truth
/// (GENERATION_LEDGER.md § Data lifetime).
///
/// @param w      Read-only world — the FINAL tile state (composition, landform,
///               deposits) the record's intermediates are joined against.
/// @param s      UI state; carries the active body and each section's disclosure
///               flag (`gen_*_open`, so a verify script can drive it).
/// @param report The world's generation report (app::m_generation_report) — the
///               profile, the Continents output, and the tile-pass inputs.
/// @param p_open Open/closed flag; toggled by the nav-rail slot.
void draw_generation_ledger(const world& w, ui_state& s,
                            const generation_report& report, bool* p_open);

} // namespace ui
