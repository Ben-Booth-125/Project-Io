#pragma once

#include "../world/standing.hpp"
#include "../world/world.hpp"
#include "ui_state.hpp"

#include <string>
#include <vector>

namespace ui {

/// Which stance group a drawn row landed in. Order is the draw order.
///
/// The three words are `docs/GLOSSARY.md`'s and `docs/politics/RELATIONS.md` § 1's —
/// stance is *hostility* and *friendship*, so the groups are Friends / Hostile /
/// Neutral. "Rival" is deliberately NOT used as the hostile group's name: in this
/// codebase a rival is any non-player corporation, and most of them are neutral.
enum class corp_stance_group : int
{
    friends = 0, ///< `are_friends(player, corp)` — symmetric, mutually chosen.
    hostile = 1, ///< `is_hostile` in EITHER direction.
    neutral = 2, ///< The remainder.
};

/// Verify-only read-back of one row the panel actually DREW last frame, with the
/// screen position of each control on it.
///
/// It exists because a capture cannot answer either question this surface's check
/// must ask. (1) *Was this row drawn at all?* — the panel filters background firms
/// out, and "eighty-odd rows became a handful" is an assertion about the population,
/// not about pixels. (2) *Did the press land?* — a control laid out past the fold-out
/// column's clip rect is INVISIBLE to `expect_no_clipping` (NR-663: table cells and
/// button labels are not instrumented into the overflow ledger), so the only honest
/// test is to synthesise a press at the rect the panel placed the control at and
/// require the world to change. A clipped control fails ImGui's hit-test and the
/// assertion fails with it — which is the failure mode this read-back is for.
///
/// Positions are ImGui screen pixels, at the centre of the named control; `{0,0}`
/// means the control was not drawn on this row.
struct corp_panel_row
{
    entity_id         corp  = null_entity;
    std::string       name;
    corp_stance_group group = corp_stance_group::neutral;
    bool              is_player     = false;
    bool              is_background = false; ///< Always false in a correct build — asserted, not assumed.
    bool              expanded      = false; ///< Row's action strip is open.

    float x = 0.0f, y = 0.0f;                 ///< Name selectable.
    float caret_x = 0.0f, caret_y = 0.0f;     ///< Disclosure arrow that opens the action strip.
    float declare_x = 0.0f, declare_y = 0.0f; ///< "Declare Hostile" (opens the inline confirm).
    float confirm_x = 0.0f, confirm_y = 0.0f; ///< The confirm's "Declare".
    float offer_x = 0.0f, offer_y = 0.0f;     ///< "Offer Friendship".
    float accept_x = 0.0f, accept_y = 0.0f;   ///< "Accept Friendship".
    float neutral_x = 0.0f, neutral_y = 0.0f; ///< "Return to Neutral".
};

/// The rows drawn by the most recent `draw_corporation_panel` call, in draw order.
/// Empty while the ledger is closed. Read by the verify seam only.
const std::vector<corp_panel_row>& corporation_panel_last_rows();

/// Draws the slot-8 all-corporations ledger — the named corporate field, grouped by
/// stance.
///
/// The population is the NAMED field: corporations with `is_background == false`,
/// plus the player's own row. Background firms (BL-365, background firms) exist to
/// fill a body's production gap and are not parties the player can stand toward;
/// listing them buried the handful of firms that matter under eighty-odd that do not.
///
/// @param w         The world to read corporations and stance from (const; mutation is
///                   via `s.pending_order_commands`).
/// @param standings This tick's per-corp standing profile, precomputed by the caller via
///                   compute_corp_standings. Only `capital_balance`/`capital_disclosed`
///                   are printed here — capital exact where the firm files, a dash where
///                   it does not, exact always for the player's own row
///                   (FINANCE.md § Disclosure). Reach and share are no longer shown: at
///                   this column width they cost the firm's NAME, and the name is the one
///                   thing a diplomacy surface cannot do without.
/// @param s         Shared UI state; clicking a row sets s.selected_entity.
/// @param open      Visibility flag; the window's close button writes false.
void draw_corporation_panel(const world& w, const std::vector<corp_standing>& standings,
                             ui_state& s, bool& open);

} // namespace ui
