#pragma once

#include "components.hpp" // resource_type
#include "entity.hpp"
#include "nation_budget.hpp"

#include <map>
#include <vector>

struct world;

// ---------------------------------------------------------------------------
// The space programme (BL-644) — the tenth budget line's consumer
// ---------------------------------------------------------------------------
// Government satellite launches: the state buys `spacecraft_components` and
// `propellant` and LAUNCHES them — a terminal sink, the first automatic buyer
// either good has (the BL-648 red list's eleventh entry claimed BL-350
// procurement as its sink, but procurement is a resource-agnostic transfer
// between two corps' pools; it moves goods without consuming them). This is
// the State demand channel's second line (MARKETS.md § Demand channels;
// NATIONS.md § A budget owns the design).
//
// THE PURCHASE IS PROCUREMENT-SHAPED, NEVER A MARKET ORDER. The strategic
// reserve's own rule (NATIONS.md): a named supplier, a direct deal, priced at
// the supplier body's market price (resolved, or base_price before first
// resolution) — the same "what the good actually costs here" basis
// `request_quote` prices from — and the credit is `run_national_budget`'s own
// direct nation→corp transfer. Nothing here bids, clears, or touches a
// market's demand array.
//
// STATE DEMAND ARRIVES IN LUMPS (NATIONS.md). A purchase claim is EARMARKED —
// `line_takes_subject` names `space_programme`, subject = the body the goods
// stand on — so rule 3a pays it whole or skips it whole. A nation whose share
// cannot yet cover a lump buys nothing, banks the difference (rule 2), and
// fires when the accumulated treasury covers a whole launch lot. That lumpy
// cadence, on a weight a rival can lobby to move, is what makes the line worth
// playing politics over rather than merely scaling into.
//
// THE GOODS ARE CONSUMED. Settlement draws the lump from the supplier's
// (corp, body) pool and credits it to nobody: the satellite launched. The
// credit half stays on the supplier's balance — it is a sale, reported through
// `corp_budget::subsidies` so `net()` explains the delta.
//
// THE PLAYER'S CORP IS NEVER A SUPPLIER. A state purchase removes goods from
// the supplier's pool without a command from its owner; on a rival that is the
// BL-202/BL-203 grant's reach, but on the player's corp it would be a forced
// sale — an auto-action the standing rules do not sanction (the one sanction
// is the `workforce_auto` dial). So `world::player_entity` is skipped at
// derivation, exactly as `corp_ai` never scores it. (Delegated call, flagged
// in the BL-644 report: this cut skips the player even under spectate.)
//
// DETERMINISM. Derivation walks `w.nation_budgets` (std::map, ascending
// nation) and `w.corp_body_pools` (std::map, ascending (corp, body));
// settlement walks the transfer record in its stored order. No RNG, no clock,
// no unordered accumulation. The share formula is nation_budget.cpp's own,
// character for character, so a claim the derivation gates as affordable is
// the claim rule 3a pays.
// ---------------------------------------------------------------------------

/// Tunables for the space programme's purchase lumps. Authored in
/// scripts/economy.lua (`economy.space_programme`); both default ZERO, so an
/// unauthored world derives no claim and runs bit-identical to the pre-BL-644
/// build — the `nation_budget` inertness discipline.
struct space_programme_params
{
    float components_lump = 0.0f; ///< `spacecraft_components` per purchase.
    float propellant_lump = 0.0f; ///< `propellant` per purchase.
};

/// One state purchase, from intent to outcome — reported, never stored. The
/// State demand channel's census surface: `funded == false` is demand the
/// budget could not pay this tick; `completed` is goods actually consumed.
struct space_purchase
{
    /// BL-742: when `supplier` is `null_entity` the counterparty is THE MARKET
    /// — the lump comes whole from `market`'s real inventory, the treasury is
    /// debited directly (the standing unbacked-market simplification), and no
    /// budget claim rides the transfer machinery. See network_upkeep.hpp's
    /// twin comment for the full rationale.
    entity_id     nation   = null_entity; ///< The buying nation.
    entity_id     supplier = null_entity; ///< The corp whose pool held the lump; null = THE MARKET.
    entity_id     body     = null_entity; ///< Where the goods stand — the claim's earmark subject.
    entity_id     market   = null_entity; ///< BL-742: the inventory market, when supplier is null.
    resource_type resource = resource_type::spacecraft_components;
    float         quantity = 0.0f; ///< The lump; leaves the pool whole on completion.
    float         credits  = 0.0f; ///< quantity x the supplier market's unit price — the claim amount.
    bool          funded    = false; ///< Rule 3a paid the claim (whole, by construction).
    bool          completed = false; ///< The goods left the pool and ceased to exist.
};

/// Derive this tick's space-programme purchase claims, BEFORE
/// `run_national_budget`. For each nation with a positive `space_programme`
/// share, and each good with a positive lump: pick the (corp, body) pool
/// holding the most unreserved stock covering a WHOLE lump (ties to the lowest
/// key; the player's corp never eligible), price the lump at that body's
/// market, and — when the line's remaining share covers it — append one
/// earmarked claim to @p claims and one intent to the returned vector. Stock a
/// claim names is reserved against later nations in the same derivation, so
/// two treasuries never buy the same units.
///
/// Emits nothing — inertly — for: unauthored lumps, a nation with no budget /
/// weight / treasury, no pool holding a whole lump, a supply body with no
/// market, or a share the lump exceeds.
///
/// @param w       World. READ ONLY here; the spend and the draw come later.
/// @param budgets Authored weights per nation — the same map the spend pass is
///                given (`run_national_budget`'s own parameter shape), so the
///                derivation and the pass cannot read two different vectors.
/// @param p       Authored lump sizes.
/// @param claims  This tick's claim list (`economy_report::budget_claims`);
///                derived claims are appended in walk order.
/// @return        The intents, in emission order — settle these after the
///                budget pass has decided which were paid.
std::vector<space_purchase> derive_space_programme_claims(const world& w,
                                                          const std::map<entity_id, nation_budget>& budgets,
                                                          const space_programme_params& p,
                                                          std::vector<budget_claim>& claims);

/// Settle the derived intents against what `run_national_budget` actually
/// paid. A paid transfer on the `space_programme` line funds the first
/// matching intent (same nation, supplier, body, bit-equal credits — rule 3a
/// paid it whole, so equality is exact); settlement then draws the whole lump
/// from the supplier's pool — the goods CEASE TO EXIST — and marks the intent
/// `completed`. A funded intent whose pool can no longer cover the lump (the
/// derivation's reservation makes this unreachable in one pass; defended
/// anyway) is CLAWED BACK exactly as a failed survey earmark is: balance
/// debited, treasury re-credited, the same two floats the pass moved.
///
/// @param w         World; supplier pools, and on claw-back the two balances,
///                  are mutated. Nothing else.
/// @param purchases The intents `derive_space_programme_claims` returned;
///                  `funded` / `completed` are written in place.
/// @param tick      The budget pass's report for the same tick.
void settle_space_purchases(world& w,
                            std::vector<space_purchase>& purchases,
                            const national_budget_tick& tick);
