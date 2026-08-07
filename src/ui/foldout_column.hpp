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
/// W = clamp(round(0.20 * disp_x), 380, 460). ~380 @1280–1920, ~410 @2048. Runtime-
/// computed from the display width (not a compile-time constant) so it stays legible
/// across the display-robustness range (DEVELOPMENT_PRACTICES § Display environment).
/// The floor is deliberate — it is the constraint that forces the one-question-per-view
/// panel splits (BL-117..121) rather than leaving them optional.
float shell_column_width(float disp_x);

/// Width and height (px) of the minimap box in the bottom-right chrome corner.
/// Lives here, with the rest of the screen geometry, because the bottom strip's
/// height is DERIVED from it (see `selection_band_height`) — when the two were
/// independent numbers they silently drifted out of alignment above a 1200px
/// minimum display dimension, which is exactly the discordance the bottom row
/// was reported for (Ben, 2026-07-30).
///
/// `mm_w = max(336, 0.28 * min(disp_x, disp_y))`, `mm_h = 0.75 * mm_w` (the 4:3
/// ratio of the original 240x180 default). 336x252 at every display whose
/// smaller dimension is <= 1200.
float minimap_width(float disp_x, float disp_y);
float minimap_height(float disp_x, float disp_y);

/// Margin (px) between the minimap box and the screen edges. The bottom strip is
/// flush to the bottom, so this is the amount the strip's top edge must sit BELOW
/// the minimap's to make the two align.
inline constexpr float chrome_margin = 8.0f;

/// Height (px) of the bottom strip — the Selection band and the comms dock, which
/// share it by design (BL-213/BL-227).
///
/// Derived so the strip's top edge lands exactly on the minimap's top edge, making
/// the screen's bottom row read as ONE band across the full width: the strip is
/// flush to the bottom while the minimap floats `chrome_margin` above it, so the
/// strip is that much taller. Was a flat 340px until 2026-07-30, which overhung the
/// minimap by 80px at 1720x1080 and left the row looking, in Ben's words,
/// discordant — with the main canvas practically blocked.
float selection_band_height(float disp_x, float disp_y);

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

/// The canvas region — BL-265's task 1: "there is currently no canvas rect in
/// the codebase to take." Bounded by the shell column on the left, the header
/// strip on top, and the bottom strip (Selection band / comms dock); the
/// minimap floats inside this region at its bottom-right corner rather than
/// reserving a column of its own. A full-canvas takeover (a ledger's `›`
/// control, per BL-265) is sized/positioned to this rect rather than the
/// whole window, so the shell chrome, header and Selection band survive.
/// First consumer: the tech-tree viewer (BL-310). Migrating the pre-BL-265
/// call sites onto this (header_panel HUD anchor, ledger_chrome) is a
/// follow-on, not done by this addition.
foldout_rect canvas_rect();

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
