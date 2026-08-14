#pragma once

#include "ui_state.hpp"

struct world;

namespace ui {

/// The AI decision feed (BL-407) — a fold-out ledger over the two stores that
/// record every strategic decision the scorer takes, and which nothing has ever
/// read.
///
/// The data is not new. `corp_decision` has carried `winning_score`,
/// `runner_up` and a `corp_decision_reason` since BL-202, pushed to
/// `world::ai_decisions` (a 256-entry ring) and, permanently, to
/// `world::history_log` under the `decision` and `agency` topics. Before this
/// surface, a grep for those types across `src/ui` + `src/core` hit only
/// `session_history.cpp` — the rationale accumulated every tick of every
/// session and was never shown to anybody.
///
/// **The margin is the point.** A command taken at 0.81 against a runner-up of
/// 0.79 is a coin-flip the tuning could have gone either way on; one taken at
/// 0.90 against 0.10 is a conviction. Nothing else in the game distinguishes
/// them, and that difference is most of what "which strategy is it running"
/// means.
///
/// Read-only by construction: it takes `world` by const reference and mutates
/// nothing but its own `ui_state` filters.
///
/// @param w    World — read for the decision ring, the history log and names.
/// @param st   UI state — the corp/reason filters and the open flag's home.
/// @param open The ledger's open flag (`ui_state::show_decision_feed`); the
///             window's own close control and the toggle rule write through it.
void draw_decision_feed(const world& w, ui_state& st, bool* open);

} // namespace ui
