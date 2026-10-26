#pragma once

#include "world/building_profit.hpp" // building_profit — the per-building figure the roster sums
#include "world/economy_system.hpp" // economy_report
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <imgui.h> // ImVec4 — the status colour this header hands out

#include <functional> // the hover card's content closure (BL-1365)
#include <string>
#include <vector>

struct ui_state; // forward-declared; the full definition lives in ui_state.hpp.

namespace ui {

// --- The construction mirror (BL-095; corrected and unified by BL-1342) ------
//
// THE CONTRACT (docs/ui/ledgers/construction.md § "Construction is durative and
// material-gated": `construction_rate` / `construction_status` "mirror
// run_construction's formula so the panel and the Selection front door show the
// same number"). This is the ONE declaration of that mirror: before BL-1342 there
// were two private copies — one in construction_panel.cpp, one in
// selection_panel.cpp — and BOTH had drifted off `run_construction`
// (src/world/economy_system.cpp, `site_rate`), reading the per-tick FLOW
// (`market_component::supply`, reset every tick) where the sim draws the
// persistent STOCK (`inventory`), omitting construction capacity from the need
// row, and ignoring the fair-price ceiling. The result was an honest-looking "ETA
// 2 qtr" over a build making no progress at all (Ben, playtest 2026-10-16).
//
// Keep the mirror and the sim's formula in step by hand: `read_construction` in
// construction_panel.cpp carries a line-by-line note against `run_construction`.
// ONE DIVERGENCE REMAINS, deliberately: the sim's pro-rata rationing of a
// CONTENDED shelf (`plan_short_shelves`, BL-1209) is a solver over every site in
// visit order and is not duplicated here, so where several sites draw one short
// shelf the mirror reads the rate the site would get alone — optimistic, never
// pessimistic. Closing that gap wants the sim to publish its realised rate in
// `economy_report`, not a second solver in the UI.

/// What `run_construction` will do with this site (or would do with this
/// candidate) on the next economy tick, and WHY — so a stall can name the good
/// that is holding it instead of saying only "Paused".
struct construction_read
{
    float         rate    = 1.0f;  ///< The sim's `site_rate`, in [0,1]; 0 is paused.
    bool          paused  = false; ///< rate == 0 — no progress at all this tick.
    bool          waiting_on_funding = false; ///< `state_idle`: a state site its treasury could not fund.
    bool          has_binding = false;        ///< A good is holding the rate below 1.
    resource_type binding = resource_type::iron_ore; ///< That good (valid only with `has_binding`).
    bool          binding_priced_out = false; ///< It is ON the shelf but over the fair-price ceiling
                                              ///< (`shelf_admits`), rather than simply short.
    bool          no_market = false;          ///< The tile routes to no market: nothing can be drawn.
};

/// The mirror itself. @p state_idle is the building's own `state_idle` (false for
/// a prospective candidate, which has no building yet).
construction_read read_construction(const world& w, const recipe_registry& reg,
                                    building_type type, resource_type target,
                                    std::uint16_t recipe, entity_id tile,
                                    bool state_idle = false,
                                    int days_left = 90);
// @p days_left (Ben, 2026-10-24; PRODUCTION.md § Construction is measured in days): the
// site's days of work left. In its last quarter (< 90) a site needs only that share of a
// quarter's materials, exactly as run_construction's need row; a candidate passes none.

/// The rate alone, for the callers that only rank/warn on it.
float construction_rate(const world& w, const recipe_registry& reg,
                        building_type type, resource_type target, std::uint16_t recipe,
                        entity_id tile);

/// DAYS of work left at this rate (`days_left` stretched by the rate); -1 when paused,
/// because a paused build has no honest ETA. Construction is measured in days (Ben,
/// 2026-10-24; PRODUCTION.md § Construction is measured in days).
int construction_eta_days(const construction_read& rd, int days_left);

/// A compact day count — "1 day", "180 days".
std::string days_label(int days);

/// A compact quarter count — "3 qtrs", "8 qtrs (~2 yr)". The display word is **qtr**,
/// never tick (NR-002, Ben 2026-08-01).
std::string ticks_label(int ticks);

/// The blocking good in a few words — "no Steel", "Steel priced out", "" when
/// nothing is holding the build. For a narrow reading (a stack button).
std::string construction_blocker_phrase(const construction_read& rd);

/// The stall's REASON CLAUSE, in the authored words and nothing else: "no Steel
/// at the market", "Steel is priced above the build ceiling", "no market
/// supplies this tile", "its treasury could not fund it this quarter", "market
/// can't supply materials". "" when nothing is stalling the build.
///
/// ONE source for the clause, so the two tenses cannot drift (BL-1365): a site
/// says "Paused - <clause>" (`construction_status`) and a candidate the player
/// has not committed to yet says "Will stall - <clause>" in its hover card.
std::string construction_stall_reason(const construction_read& rd);

/// The full status line: "Building... ~180 days", "Building... ~810 days (short of
/// Steel)", "Paused - no Steel at the market", "Paused - Steel is priced above
/// the build ceiling". @p days_left is the site's days of work left.
std::string construction_status(const construction_read& rd, int days_left);

/// Red paused / amber stretched / green on schedule.
ImVec4 construction_status_colour(const construction_read& rd);

/// The colour for a bare rate (the Build door's pre-commit warning, which has a
/// candidate rather than a site).
ImVec4 construction_rate_colour(float rate);

/// How far through its build @p b is, in [0,1] — `construction_paid` over
/// `construction_capex`, the CREDIT fraction. Preferred over the ticks ratio
/// because `ticks_remaining` is measured against a total nothing stores: recomputing
/// it calls `construction_site_multiplier`, which reads the live reach/stack field,
/// so the denominator moves when a road or the stack does. Capex is re-priced live
/// too, so the bar can wobble with local prices; the drift-free alternative is to
/// store the original total on the building, which is a save-format change.
/// Falls back to the ticks ratio where capex cannot be priced.
float construction_progress_fraction(const world& w, const recipe_registry& reg,
                                     const building_component& b);

/// The NAME the Buildings view files @p b under — the same words the Build door
/// offers, so a player recognises the row as the thing they built:
/// `extraction_building_name(target_resource)` for an extraction site ("Quarry";
/// "Well" where `placement_rules::is_well_site` holds, BL-1198, and "Fishing
/// Wharf" where `placement_rules::is_wharf_site` does, BL-1199 — so it needs @p w),
/// the active recipe's own `group` for a processing facility ("Metal Foundry", the
/// BL-434 grouping vocabulary), and `building_type_name` for everything else.
///
/// The grouping key is the display name itself, deliberately: two buildings the
/// player would call the same thing belong on the same row, and nothing finer is
/// visible to them.
std::string building_group_name(const world& w, const recipe_registry& reg, const building_component& b);

// --- The one-line GRID row, and its hover card (BL-1365) ---------------------
//
// construction.md § "One line a row, and the words live in the hover" and § "The
// line is a grid, not a sentence" (Ben, 2026-10-17). Every construction row — the
// build list, the queue, the tile card's stack buttons, the expanded site card —
// is ONE LINE of FIXED COLUMNS, reading in glyphs, and everything the line drops
// goes to the shared hover card (../docs/ui/TOOLTIP.md's `draw_hover_card`), never
// to a second line and never to nothing.
//
// These three helpers are what makes the four surfaces one treatment rather than
// four look-alikes: the same row metric, the same hit-test, the same card frame.

/// The grid's FIXED column widths, in pixels — shared by the four surfaces so a
/// column is the same column on each of them, and so the budget is stated in one
/// place rather than re-guessed per table.
///
/// Measured against the fold-out column's narrowest case: `shell_column_width`
/// floors at 380 px, less `nav_pane_width`, the ledger's window padding, the
/// build list's own scroller and scrollbar and a `TreeNodeEx` indent leaves the
/// build list ~265 px. At that width NINE columns do not fit, so the MATERIALS
/// column is the one dropped into the hover card (construction.md's rule: drop
/// the lowest-value column, never wrap and never widen the panel) — the credit
/// cost beside it already prices exactly those materials at the local market, and
/// the quantities, which are the half a player can act on, were never on the line
/// at all. The hover names them in full.
namespace construction_grid {
inline constexpr float glyph = 15.0f; ///< The building glyph.
inline constexpr float cost  = 34.0f; ///< Credits, abbreviated ("940", "1.2k").
inline constexpr float makes = 14.0f; ///< The resource pip for what it makes.
inline constexpr float stack = 28.0f; ///< "2/4".
inline constexpr float state = 12.0f; ///< The state mark.
inline constexpr float bar   = 38.0f; ///< The profit / progress bar.
inline constexpr float act   = 44.0f; ///< The Build press.
inline constexpr float eta   = 32.0f; ///< Days left (the queue) - sized so its
                                      ///< "Days" header is not itself elided.
inline constexpr float pad   = 2.0f;  ///< Horizontal cell padding, per side.
/// HOW MANY TEXT LINES A ROW RESERVES (BL-1371) — two, since Ben's live click of
/// the one-line grid: *"we are going to have to use two row spaces for each
/// building, this is so that text can wrap to the second row."* A column whose
/// text does not fit wraps into the second line instead of eliding; a row that
/// needs only one still occupies two, so the grid stays a grid. The one place
/// that knows the number is `construction_row_height`, which reads it from here.
inline constexpr int   lines = 2;
} // namespace construction_grid

/// The height of ONE construction grid row: `construction_grid::lines` text
/// lines plus the cell padding. Every one of the five surfaces measures against
/// this, so a row cannot quietly grow back to the seven lines BL-1344 left
/// behind, and the two-line budget BL-1371 bought is stated in ONE place.
float construction_row_height();

/// Draw a grid cell's text, wrapping into the row's second line rather than
/// eliding (BL-1371). A thin wrapper over `ui::fit_wrapped_text` at
/// `construction_grid::lines`, so no surface has to restate the budget.
/// @return the number of lines drawn; @p elided is set only when even the whole
///         budget could not hold the string.
int construction_grid_text(const char* site, const char* text, float max_w,
                           bool disabled = false, bool* elided = nullptr);

/// The grid's BAR: a track and a fill, and no figure at all.
///
/// `charts::draw_value_bar` cannot serve a grid column — it reserves at least
/// 52 px for the number it prints right-aligned, so inside a 38 px column the
/// plot width goes negative and the "bar" is the figure alone. That is the one
/// thing this pass is undoing: the figure is a word, and words live on the
/// hover card. @p frac is the already-normalised magnitude in [0,1]; a zero
/// keeps a 2 px stub so zero reads as zero rather than as an absent bar.
void construction_grid_bar(ImDrawList* dl, ImVec2 mn, ImVec2 mx, float frac, ImU32 colour);

/// Is the pointer over the row occupying [@p mn, @p mx]?
///
/// A hit-test rather than an ImGui item, deliberately: a row-wide `Selectable`
/// would claim the click that the row's own Build press needs, and an
/// `InvisibleButton` would advertise a press the build list does not have. Gated
/// on the host window being hovered so a card cannot appear through the ledger
/// above it.
bool construction_row_hovered(ImVec2 mn, ImVec2 mx);

/// The shared hover card, drawn for a construction row: `ui::draw_hover_card` at
/// the live cursor with @p content as its body. @p title is drawn first in the
/// selection ink (the row's name, which the grid itself may have elided).
void construction_row_card(const char* title, const std::function<void()>& content);

// --- EACH ELEMENT EXPLAINS ITSELF, AND ONLY ITSELF (BL-1368) -----------------
//
// construction.md § "Each element explains itself, and only itself" (Ben,
// 2026-10-17): *"I would expect the tooltip to be shorter, and give each element
// in the row a different explainer."* BL-1365's card answered every column at
// once, which was right against its own brief and too long in the hand — eight
// lines to read one figure. The hover is therefore PER COLUMN: the cost column
// explains the cost, the pip says what the building makes, the stack figure says
// WHICH cap it counts against, the state mark names the good that stalls it, the
// bar explains the estimate, the press explains a refusal.
//
// A column's explainer is ONE OR TWO SHORT SENTENCES. If it wants more, the
// column is carrying two facts and should be two columns or one fact.
//
// The hit-test is a RECT per cell, not an ImGui item, for the same reason the row
// hit-test was: an item over the cell would claim the click the Build press needs.
// The act column is hit-tested exactly like the others, so a disabled Build still
// explains its refusal while an enabled one still takes the press.

/// One column's own explainer: the cell it occupies, and the sentences that
/// explain THAT column and nothing else. An empty @ref text means the column has
/// nothing to say on this row and shows no card at all.
struct grid_explainer
{
    const char* key = "";          ///< Stable column name, published for the checks.
    ImVec2      mn{0.0f, 0.0f};    ///< The cell's rect, in screen px.
    ImVec2      mx{0.0f, 0.0f};
    std::string text;              ///< One or two short sentences.
    ImU32       ink = 0;           ///< 0 = the disabled ink; otherwise the body's colour.
};

/// The cell occupied by a column @p w px wide drawn at @p cursor (the
/// `GetCursorScreenPos` inside the cell), grown by the grid's own cell padding so
/// neighbouring columns abut with no dead strip between them.
grid_explainer construction_cell(const char* key, ImVec2 cursor, float w, std::string text,
                                 ImU32 ink = 0);

/// Hit-test @p cells in order and draw the shared hover card for the FIRST one
/// the pointer is inside, titled @p title (the row's name, which the grid may
/// have elided). Returns the index drawn, or -1 for none.
int construction_column_card(const char* title, const std::vector<grid_explainer>& cells);

/// Every column's explainer, newline-joined — the union a check asserts
/// "nothing was lost from the row-wide card" against, in one string.
std::string construction_cells_text(const std::vector<grid_explainer>& cells);

/// The BUILD LIST's eight columns, declared ONCE: glyph | name | cost | makes |
/// stack | state | bar | act.
///
/// Shared so the rail rows (`rail_tool.cpp`) are the SAME grid as the
/// Infrastructure rows they sit between rather than a second idiom beside them
/// (BL-1366: they were a four-line bordered box, visibly taller than the one-line
/// rows above and below). Push/pop the cell padding itself; `construction_end_grid`
/// undoes both.
bool construction_begin_build_grid(const char* id);

/// Close a table opened by `construction_begin_build_grid` (or any grid that
/// pushed the shared cell padding).
void construction_end_grid();

/// One row of the Buildings view: a building TYPE the player owns at least one of.
struct building_group
{
    std::string            name;    ///< Display name — the grouping key.
    int                    count = 0;   ///< How many the player owns.
    float                  total = 0.0f; ///< Sum of the members' per-quarter net profit.
    std::vector<entity_id> members;  ///< The buildings themselves, best net first.
};

/// The player's estate grouped by type — one entry per named building type the
/// player owns at least one of, each carrying a count and the summed per-quarter
/// net profit of its members.
///
/// `count` is every player building of that type, INCLUDING one still under
/// construction; `total` sums only the members `estimate_building_profit` can
/// actually price (`has_data`), a building with no estimate contributing zero
/// rather than a fabricated figure. The per-building number is the same one
/// `rank_player_buildings_by_profit` ranks the Balance ledger on, so the two
/// surfaces cannot report different profits for the same building.
///
/// Deterministic: groups sorted by name, members by net descending then by entity
/// id, so the walk over `world::buildings` (an unordered container) cannot vary the
/// order a player or a check sees.
std::vector<building_group> player_building_groups(const world& w,
                                                   const recipe_registry& reg,
                                                   const economy_report& report);

/// Draw the **Construction ledger** — nav rail slot 3, a button-strip nav across two
/// bounded views:
///
///  - **Buildings** (the default) — the player's estate grouped by type, one row per
///    type with a count and a total profit. Expanding a row lists that type's own
///    buildings; pressing one selects it and draws its levers (Method, Workforce),
///    which live here since Ben's 2026-08-29 ruling that the Selection card's centre
///    presents and never operates. It is the default because the queue is empty most
///    of the time and the player always owns buildings — opening on an empty queue
///    makes the ledger's front door an empty room.
///  - **Construction** — the collapsible build queue, then the tile-selection build
///    bar (`draw_construction_ledger_body`, selection_panel.cpp). ONE construction
///    element with TWO DOORS: this tab is what the rail slot opens and what the tile
///    Selection element's Construct button opens, so there is a single build bar.
///
/// Since BL-122 this is re-hosted into the shell fold-out column (foldout_column.hpp),
/// like the other named ledgers: it draws pinned + borderless into the shared column
/// rect, and is closed via the nav rail (accordion) rather than a title-bar 'x'. The
/// former BL-082 caller-supplied spawn/height-cap is gone — the column sits entirely
/// left of the Selection element, so no bottom-clearance anchoring is needed.
///
/// When *p_open is false the function draws nothing.
///
/// @param w          World — read for building/tile lookup; written by management
///                   controls (workforce_target, decommissioned, active_recipe_index).
/// @param reg        Loaded registry (recipe names and per-type economics).
/// @param report     Latest economy report (BL-143) — per-building active/idle status
///                   for the Buildings tab's status column.
/// @param state      Shared UI state — read (selected_entity) and written (construction).
/// @param p_open     Open/closed flag; gates whether the panel draws.
void draw_construction_panel(world& w,
                             const recipe_registry& reg,
                             const economy_report& report,
                             ui_state& state,
                             bool* p_open);

} // namespace ui
