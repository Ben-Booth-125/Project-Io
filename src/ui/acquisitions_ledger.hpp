#pragma once

#include "ui_state.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

namespace ui {

/// Draw the Acquisitions ledger — nav-rail slot 5, and the Company lens's click
/// destination. One question: **which firms can I buy, and what do they cost?**
///
/// ── THE FIELD WAS MEASURED BEFORE THIS WAS LAID OUT, THEN IT MOVED ───────
/// `tools/verify/acquisition_viability.cpp` § C, twelve seeds of the shipped
/// spawn, 88 corporations each. The buyable field is **a mean of 81.6 firms**
/// (Purchasable 35.8, Possible 45.8), and one of the two groups is empty on no
/// seed at all.
///
/// It was **1.6** when this surface was first laid out, and the layout still
/// carries that inheritance: the field size is printed on the surface's own
/// face, and the two groups were kept rather than collapsed. Both decisions
/// were made for a one-row list and both survive a long one, which is the
/// argument for having measured rather than assumed.
///
/// The binding constraint was never filing — public-but-unfiled is **zero on
/// every seed**, so `buy_corporation`'s gate (4) has never bound. It was
/// ownership class, and companies stopped carrying one: closure is retired for
/// background firms (`FINANCE.md` § Who may be bought), so the mean `closed`
/// count fell 84.8 → 4.8 and only corporations withhold now.
///
/// This surface is therefore small by construction, and it is deliberately NOT
/// padded to look otherwise. Two consequences are designed for rather than
/// hidden: the field's size is stated on the surface, so two rows read as the
/// world and not as a bug; and five of twelve seeds price their one buyable
/// firm at exactly ZERO — the free-firm trap FINANCE.md § Whole-firm
/// acquisition documents (BL-658) — so a zero price says on hover what it costs.
///
/// The substance is in the fold-out instead: 85.4 firms per seed file a return
/// in world state that they do not disclose, so the profitability table's ~88
/// rows are where this ledger's reading actually is.
///
/// ── THE COLUMN ───────────────────────────────────────────────────────────
/// Two collapsing groups over the buyable field, both open at rest (a mean of
/// two rows total means a collapsed group hides the whole answer):
///
/// - **Purchasable** — priced at or below the player corporation's balance.
///   These rows carry the Buy press.
/// - **Possible** — priced, but beyond the balance today. Price shown, **no
///   press**. That contrast is the entire reason the second group exists: the
///   player must be able to see what the next rung costs.
///
/// A firm that does not file **does not appear at all** — it cannot be priced,
/// and `apply_corp_command` refuses it (FINANCE.md § Whole-firm acquisition:
/// "A public firm that has never filed cannot be priced, and is refused"). No
/// stance, reach or share column: ownership moves whole, by buyout, and there
/// is no fractional stake, share count or controlling-holder threshold anywhere
/// in the model.
///
/// ── THE PRESS ────────────────────────────────────────────────────────────
/// Buy enqueues `corp_verb::buy_corporation` onto `ui_state::pending_order_
/// commands`, the same deferred-dispatch seam every other ledger press uses —
/// never a direct write. The verb is used UNCHANGED: it already prices at
/// `max(0, book_value + k_acquisition_multiple x trailing_net + balance)` and
/// already gates on not-the-player, `publicly_held`, has-filed and solvency.
/// It does **not** test `is_background`, and this surface adds no such gate:
/// the buyable field is every public filed firm, which in practice is mostly
/// background companies.
///
/// ── THE FOLD-OUT (BL-627) ────────────────────────────────────────────────
/// A `›` control opens a full-canvas takeover (`detail_surface::acquisitions_
/// profit`, the BL-214/BL-265 idiom the Corporation dashboard's roll-up cards
/// use) holding the profitability table: one row per corporation, sortable on
/// every column — type, end resource, input resource, profit, price, ownership
/// class. Exact where the firm files; a dash where it does not; no bands
/// anywhere (FINANCE.md § Disclosure, Ben 2026-08-26: the banded standing read
/// is retired). It is a takeover rather than an in-place expansion because ~88
/// rows over six columns do not fit a 380 px column at any font size.
///
/// @param w    Read-only world — corporations, their filed returns, holdings
///             and recipes.
/// @param reg  Loaded registry: `acquisition().multiple` is the price formula's
///             authored term, and the recipe table resolves each firm's end and
///             input resources.
/// @param s    UI state; read for the group/sort/focus state, written by the
///             Buy press (which enqueues onto `pending_order_commands` — this
///             surface holds only a `const world&` and cannot mutate it).
///             Named `s`, not `ui`, because this function lives IN
///             `namespace ui` (the market/balance/contracts ledgers' own
///             convention).
/// @param open Open/closed flag; toggled by the nav rail and by a Company-lens
///             click.
void draw_acquisitions_ledger(const world& w, const recipe_registry& reg,
                              ui_state& s, bool& open);

} // namespace ui
