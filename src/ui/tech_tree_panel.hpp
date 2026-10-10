#pragma once

#include "../world/nation_tech.hpp"
#include "../world/world.hpp"
#include "ui_state.hpp"

namespace ui {

// The research panel (BL-1276) — docs/economy/RESEARCH.md owns the tree's
// grammar and the queue; docs/economy/tech/DIGITISATION_TREE.md the nodes.
//
// Two halves, one flag (`ui_state::show_tech_tree`, nav slot 4 and F9):
//
//   * the COLUMN (`draw_tech_tree_menu`) — the Inspect / Plan strip, the seat's
//     nation and its research rate, and the queue with each entry's arrival
//     quarter, Remove last and Clear;
//   * the CANVAS (`draw_tech_tree_panel`) — the Digitisation tree as three trees
//     in a ring, every node and link, the shared hover card on a node, and in
//     Plan mode a press that queues the node for the seat's nation.
//
// The panel reads the world and never writes it: a press is a `corp_command`
// (`queue_research` / `unqueue_research` / `clear_research`) pushed onto
// `ui_state::pending_research_commands`, applied by `app::render` through
// `apply_corp_command`, the seam an agent's press takes. The refusal reason a
// press shows comes from `preview_enqueue`, the planner `enqueue_node` runs.

/// The seat's nation — the home nation of `world::player_entity` — or
/// null_entity when no corporation is seated or it has no nation. The nation
/// whose research the panel plans; Plan mode is disabled without one.
entity_id research_player_nation(const world& w);

/// The fold-out column (slot 4, Research). Guards its own visibility off
/// `ui_state::show_tech_tree`.
void draw_tech_tree_menu(const world& w, const research_params& p, ui_state& s);

/// The research canvas (a full-canvas takeover, ui::canvas_rect()). Guards its
/// own visibility off `ui_state::show_tech_tree`.
void draw_tech_tree_panel(const world& w, const research_params& p, ui_state& s);

} // namespace ui
