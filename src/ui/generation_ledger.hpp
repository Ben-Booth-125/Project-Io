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

/// The per-tile derivation breadcrumb, as its own content builder: height/ocean →
/// band/moisture → composition → landform → deposits, one section per pass, each
/// naming the input value and the rule that fired.
///
/// Factored out because the ledger is not its only intended caller — the hover card
/// and the Selection info element show the same causal chain in a condensed frame
/// (GENERATION_LEDGER.md § Surfacing; SELECTION.md § Shared content builders). The
/// FRAME differs; the content must not.
///
/// NO CALLER TODAY (2026-08-30). The ledger's Tile tab was retired when the surface
/// became one flat panel of sections, and it was this function's only caller — the
/// hover card and Selection wiring the paragraph above describes as its reason for
/// existing was never built. Kept rather than deleted because the content is the
/// asset and its stated destination is unchanged; that gap is the open question,
/// not this declaration. See NEEDS_REVIEW.
///
/// Draws nothing but ImGui text into the current window.
///
/// @param w       Read-only world — the tile's final state.
/// @param rec     The body's regenerated per-pass record.
/// @param entry   The body's generation report entry (profile, endowment, inputs).
/// @param tile_id The tile to explain; must belong to the body @p entry describes.
void draw_tile_derivation(const world& w, const generation_record& rec,
                          const generation_report::body_entry& entry,
                          entity_id tile_id);

} // namespace ui
