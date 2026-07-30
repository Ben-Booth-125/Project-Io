#pragma once

namespace ui {

/// A screen rect in pixels ({x, y} top-left, {w, h} extent). A small local type so
/// callers read `.w`/`.h` as width/height rather than ImVec4's `.z`/`.w`.
struct foldout_rect { float x, y, w, h; };

/// Width of the permanent left **shell column** (BL-122), in pixels. The identity tile
/// caps the column at top, the icon nav rail runs down its left edge, and a fold-out
/// ledger fills the rest when a nav slot is active. This width is what the balance bar
/// and the Selection element clear on their left.
///
/// W = clamp(round(0.17 * disp_x), 300, 360). ~300 @1280, ~326 @1920. Runtime-computed
/// from the display width (not a compile-time constant) so it stays legible across the
/// display-robustness range (DEVELOPMENT_PRACTICES § Display environment). The 300px
/// floor is deliberate — it is the constraint that forces the one-question-per-view
/// panel splits (BL-117..121) rather than leaving them optional.
float shell_column_width(float disp_x);

/// Screen rect of the fold-out panel body — the region a ledger draws into when its
/// nav slot is active. Sits to the RIGHT of the icon rail (`[nav_pane_width, W]`) and
/// runs from just below the identity tile to the bottom margin. It is entirely left of
/// the narrowed Selection element (x >= W), so it needs no bottom-clearance
/// coordination. Pure function of `ImGui::GetIO().DisplaySize`.
foldout_rect foldout_column_rect();

/// The comms dock (BL-227) — the bottom-left tile of the screen's bottom strip,
/// sharing the Selection band's top edge and height so the two read as ONE
/// horizontal bar rather than two stacked things.
///
/// Occupies the fold-out column's x-range (`[nav_pane_width, W]`), NOT the full
/// shell column: the icon rail keeps its full height and runs down the left edge
/// past the dock. That is a measured constraint, not a preference — at the
/// 1280x720 floor (DEVELOPMENT_PRACTICES § Display environment, BL-215) a
/// shortened rail would have `720 - 92 - 340 = 288` px for nine ~44 px slots plus
/// spacing (~428 px needed), so spanning the dock across the rail would clip two
/// of them. `foldout_column_rect` shortens to clear this dock, which is what makes
/// "the menu and ledgers are always shorter" true.
foldout_rect comms_dock_rect();

/// Begin a fold-out ledger window pinned to foldout_column_rect(): borderless,
/// non-moving, non-resizing, scroll allowed for overflow. Mirrors ImGui::Begin's
/// return (false when fully clipped). Pair with foldout_end().
bool foldout_begin(const char* name);

/// End a foldout_begin() window. Must be called once per foldout_begin() — including
/// the early-out path where foldout_begin() returned false — matching the
/// ImGui::Begin/End contract.
void foldout_end();

/// One tab button in a fold-out panel's button-strip nav — the one-question-per-view
/// selector shared across the BL-117.. sweep (modelled on the Construction panel's
/// Build/Manage/Sell strip). Highlights when `view == id`; sets `view = id` on click.
/// Callers place buttons with `ImGui::SameLine()` between them and a `Separator` below.
/// Used because `ImGui::BeginTabBar`'s header does not render in this build.
///
/// Toggle rule (BL-126, io-standing-rules): re-clicking the *already-active* tab closes
/// the hosting ledger via `close` (its `show_*` flag) rather than being a no-op; switching
/// to a different tab is an ordinary view change. Pass the ledger's open-flag as `close`;
/// `nullptr` (the default) keeps the strip non-closable (a plain view selector).
void nav_button(const char* label, int id, int& view, bool* close = nullptr);

} // namespace ui
