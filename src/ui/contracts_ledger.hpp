#pragma once

#include "ui_state.hpp"
#include "world/contract_template.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

namespace ui {

/// Draw the Contracts ledger (BL-576) — nav rail slot 13. Three views, the
/// standing button-strip split (`ui_state::contracts_ledger_view`,
/// LAYOUT.md § One-question-per-view splits):
///
/// - **Offers** — open `world::mercenary_offers`, filtered by the activity
///   fog (`body_activity_visibility`, CONTRACTS.md Q4: "visible only where
///   the activity fog already reaches") against the offer's target
///   province's body. Each row shows the client nation, the target
///   province, the fee (with its escrow-fill progress), the ticks to the
///   eventual contract's deadline, and the client's highest-grudge
///   neighbour's garrison strength at the target (`garrison_strength_in`) —
///   what the contractor is actually being asked to overcome. An Accept
///   press opens a force picker over the player's own uncommitted units
///   (`unit_strength` — the same derivation the Selection band's Soldier
///   card uses); Confirm enqueues `corp_verb::accept_offer` onto
///   `ui_state::pending_order_commands`, the same deferred-dispatch seam
///   every other ledger press already uses. Decline is LOCAL ONLY — nothing
///   is sent to the seam, and the offer stays open (CONTRACTS.md: an offer
///   is refusable, never dismissable).
/// - **Active** — the player's own `world::mercenary_contracts` in the
///   `active` state, predicate rendered through `condition_text` (the
///   template's predicate with `province` bound to the contract's own), and
///   the committed force. An Abandon press shows the reputation cost
///   (`reg.sentiment().factors[contract_cancelled].trust`) BEFORE it
///   enqueues `corp_verb::abandon_contract` — CONTRACTS.md's "an honest
///   early exit costs less than a rout, but it still costs".
/// - **History** — the player's terminal-state contracts (completed /
///   failed / abandoned) and what was actually paid.
///
/// @param w         Read-only world (offers, contracts, units, nations,
///                   provinces, corporations).
/// @param reg       Loaded registry — the sentiment factor table for the
///                   Abandon press's reputation-cost readout.
/// @param templates Loaded contract-template registry — the predicate each
///                   contract's kind names (CONTRACTS.md § Serialisation:
///                   a contract stores an INDEX into this table, never a
///                   free `condition_set`).
/// @param s         UI state; read for the ledger's own view tab and the
///                   force-picker's checked units, written by the Accept/
///                   Abandon presses (which enqueue onto
///                   `pending_order_commands` — this surface holds only a
///                   `const world&` and cannot mutate it directly). Named
///                   `s`, not `ui`, because this function lives IN
///                   `namespace ui` (the market/balance ledgers' own
///                   convention) — a parameter named `ui` would shadow the
///                   namespace for any qualified `ui::` call in the body.
/// @param open      Open/closed flag; toggled by the nav rail.
void draw_contracts_ledger(const world& w, const recipe_registry& reg,
                           const contract_template_registry& templates,
                           ui_state& s, bool& open);

} // namespace ui
