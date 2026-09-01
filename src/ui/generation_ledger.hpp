#pragma once

#include "ui_state.hpp"
#include "world/hard_coded_world.hpp" // generation_report — the tile-pass inputs to replay
#include "world/tile_generation.hpp"  // generation_record — the per-pass intermediates
#include "world/world.hpp"

namespace ui {

/// Draws the Generation Ledger — the developer tuning surface that explains WHY a
/// tile generated as it did. Design authority: docs/generation/GENERATION_LEDGER.md.
///
/// Two views, because the two questions differ: **Body** (the aggregate shape —
/// composition/landform histograms, the ocean threshold against the profile's
/// target, the latitude bands, and the profile that drove all of it) and **Tile**
/// (the five-step derivation breadcrumb for the selected tile).
///
/// The per-pass intermediates it reads are REGENERATED ON DEMAND from the report's
/// recorded tile-pass inputs and cached for as long as one body stays the subject;
/// they are never stored on the world and never reach the save. Generation is
/// deterministic in those inputs, so a stored copy would be bloat, not truth
/// (GENERATION_LEDGER.md § Data lifetime).
///
/// @param w      Read-only world — the FINAL tile state (composition, landform,
///               deposits) the record's intermediates are joined against.
/// @param s      UI state; carries the ledger's view index and the selected tile.
/// @param report The world's generation report (app::m_generation_report) — the
///               profile, the Continents output, and the tile-pass inputs.
/// @param p_open Open/closed flag; cleared by re-clicking the active view tab.
void draw_generation_ledger(const world& w, ui_state& s,
                            const generation_report& report, bool* p_open);

} // namespace ui
