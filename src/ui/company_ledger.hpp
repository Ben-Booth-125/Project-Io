#pragma once

#include "ui_state.hpp"
#include "world/world.hpp"

namespace ui {

/// Draw the Company ledger (BL-666) — the destination for a click on a
/// background firm's holding under the **Company lens**.
///
/// Ben, 2026-08-28: *"clicking will take you to a relevant ledger. (Different
/// types for either, make a placeholder if needed)."* Since the same day's
/// GLOSSARY split, **corporation** and **company** are different words, so the
/// two lenses cannot share a destination: a corporation (the player or a rival)
/// lands on the all-corporations table, a company (an `is_background` firm)
/// lands here.
///
/// **This surface is a declared placeholder** and says so on its own face. It
/// answers none of the five axes a real ledger design answers — top question,
/// sub-views and default, lens on open, data sources, close semantics
/// (docs/ui/ledgers/README.md) — because none of them have been decided yet.
/// What it does is narrower and deliberately so: it names the firm, states that
/// it is a company rather than a corporation, and prints the handful of facts
/// already public about any firm. It invents no figure — where a number is not
/// derivable from the live world it is absent, never a zero that reads as real.
///
/// A column occupant with **no nav-rail slot**: nothing but a canvas click opens
/// it, and `close_all_panels` closes it like every other tenant of the fold-out
/// column.
///
/// @param w    Read-only world — the firm, its home nation, and the buildings
///              its holdings count is derived from.
/// @param s    UI state; read for `selected_company` (which firm) and
///              `active_body` (which body the holdings count is about). Nothing
///              here writes to it — the surface carries no press.
/// @param open Open/closed flag, cleared by `close_all_panels`.
void draw_company_ledger(const world& w, ui_state& s, bool& open);

} // namespace ui
