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

/// Begin a fold-out ledger window pinned to foldout_column_rect(): borderless,
/// non-moving, non-resizing, scroll allowed for overflow. Mirrors ImGui::Begin's
/// return (false when fully clipped). Pair with foldout_end().
bool foldout_begin(const char* name);

/// End a foldout_begin() window. Must be called once per foldout_begin() — including
/// the early-out path where foldout_begin() returned false — matching the
/// ImGui::Begin/End contract.
void foldout_end();

} // namespace ui
