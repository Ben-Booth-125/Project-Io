#pragma once

#include "components.hpp" // resource_type
#include "entity.hpp"
#include "nation_budget.hpp"

#include <array>
#include <map>
#include <vector>

struct world;

// ---------------------------------------------------------------------------
// Network upkeep (BL-643) — the logistics_maintenance line's consumer
// ---------------------------------------------------------------------------
// A nation's road network consumes materials (stone, timber) to stay standing —
// the Infrastructure demand channel (MARKETS.md § Demand channels), a permanent
// terminal sink scaled by GEOGRAPHY (network size), never by population. The
// `logistics_maintenance` budget line has named exactly this since BL-537
// (NATIONS.md § A budget; LOGISTICS.md § 4: "the sink the logistics maintenance
// line pays into"); until this TU it spent credits on nothing.
//
// THE SHAPE IS BL-644's STATE PURCHASE, WITH ONE DELIBERATE DIFFERENCE.
// Like the space programme: a named supplier, a direct deal priced at the
// supplier body's market (resolved price, or base_price before first
// resolution), the credit moved by `run_national_budget`'s own transfer, the
// goods drawn from the supplier's (corp, body) pool and CONSUMED — repairs go
// into the roadbed and land nowhere. Nothing here bids, clears, or touches a
// market's demand array.
//
// UNLIKE the space programme, the claim is UNEARMARKED — rule 3's ordinary
// pro-rata line, never rule 3a's whole-or-nothing lump. Settled against
// NATIONS.md's own rules: `logistics_maintenance` is not a subject-taking line
// (`line_takes_subject`), and upkeep is CONTINUOUS, not lumpy — half the repair
// budget genuinely buys half the materials and keeps half the network fed,
// where half a satellite launches nothing. A whole-or-nothing fill would also
// make a poor nation bank forever while its network drew no materials at all,
// which inverts the channel's purpose (a permanent sink, income to material
// suppliers, consumption of gluts). So a transfer on this line carries a
// `fill_fraction` that may sit anywhere in (0, 1], and settlement draws
// quantity x fill — the partially-funded repair is the meaningful, reported
// case (`network_purchase::drawn` < `quantity`).
//
// WHAT AN UNFUNDED DRAW DOES: NOTHING DEGRADES. Ben's 2026-08-22 ruling
// (LOGISTICS.md § 4): "Roads do not decay... the cost is binary, not graduated"
// — a solvent nation's roads behave exactly as built, and the failure state is
// national insolvency (BL-550), not a strained-network middle band. This
// channel's economic purpose needs no degradation; whether unmet upkeep should
// ever feed the insolvency ladder is a delegated design call, flagged in the
// BL-643 report and NOT built here.
//
// THE GOODS ARE CONSUMED, the credit is a SALE: it stays on the supplier's
// balance and `run_nation_step` folds it onto `corp_budget::subsidies` so
// `net()` explains the delta — the space programme's own fold, at 4c.
//
// THE PLAYER'S CORP IS NEVER A SUPPLIER. A state purchase drains the pool
// unasked; on the player's corp that is a forced sale the standing rules do
// not sanction (the one sanction is the `workforce_auto` dial). Skipped at
// derivation, exactly as space_programme.cpp skips it.
//
// DETERMINISM. Network size is DERIVED FROM THE WORLD EACH TICK — no
// persistent state, no serialisation seam. The tallies walk `w.tile_to_nation`
// and `w.buildings` (unordered_maps) accumulating INTEGER counts only, which
// commute, so the result is independent of hash layout; every float
// accumulation after that walks std::maps in ascending key order. No RNG, no
// clock. The share is never recomputed here at all — the pass's own pro-rata
// decides funding, so derivation and spend cannot disagree.
// ---------------------------------------------------------------------------

/// Material rates for keeping the network standing, per economy tick. Authored
/// in scripts/economy.lua (`economy.network_upkeep`, flat keys `stone_track`,
/// `timber_hub`, ...); every value defaults ZERO, so an unauthored world
/// derives no draw and runs bit-identical to the pre-BL-643 build.
struct network_upkeep_params
{
    /// Per road tile, indexed by road_level - 1 (Track / Road / Highway).
    std::array<float, 3> stone_per_level{};
    std::array<float, 3> timber_per_level{};
    /// Per ACTIVE port or inland logistics hub (complete, not decommissioned).
    float stone_per_hub  = 0.0f;
    float timber_per_hub = 0.0f;
};

/// One network-upkeep purchase, from intent to outcome — reported, never
/// stored. The Infrastructure channel's census surface: `quantity` is the
/// bill the network asked for (capped at the supplier's stock), `drawn` what
/// the funded fraction actually consumed, and the gap between them is demand
/// the budget could not pay this tick.
struct network_purchase
{
    entity_id     nation   = null_entity; ///< The maintaining nation.
    entity_id     supplier = null_entity; ///< The corp whose pool held the materials.
    entity_id     body     = null_entity; ///< Where the goods stand (the pool's body).
    resource_type resource = resource_type::stone;
    float         quantity = 0.0f; ///< Units the claim asked for — the bill.
    float         credits  = 0.0f; ///< quantity x the supplier market's unit price — the claim amount.
    float         paid     = 0.0f; ///< Credits actually transferred (claim x the pass's fill).
    float         drawn    = 0.0f; ///< Units consumed: quantity x the transfer's fill_fraction.
    bool          funded    = false; ///< The pass paid the claim (possibly partially).
    bool          completed = false; ///< The drawn units left the pool and ceased to exist.
};

/// Derive this tick's network-upkeep purchase claims, BEFORE
/// `run_national_budget`. For each nation in @p budgets (ascending id): tally
/// its network — road tiles by level through `tile_to_nation`, plus active
/// ports / inland hubs standing on its territory — multiply by the authored
/// rates, and for each material with a positive bill pick the (corp, body)
/// pool holding the most unreserved stock (strict >, so ties keep the lowest
/// key; the player's corp never eligible), CAP the bill at that stock, price
/// it at the supplier body's market, and append one UNEARMARKED
/// `logistics_maintenance` claim plus one intent. Stock a claim names is
/// reserved against later nations in the same derivation.
///
/// Emits nothing — inertly — for: all-zero rates, a nation with no network,
/// no pool holding any stock, or a supply body with no market / no price
/// basis. Deliberately NOT gated on treasury, weight or share: funding is the
/// budget pass's decision, and an intent the pass could not pay is the
/// channel's honest unfunded-demand record.
///
/// @param w       World. READ ONLY here; the spend and the draw come later.
/// @param budgets Authored weights per nation — the same map the spend pass
///                reads, so the two cannot see different nation sets.
/// @param p       Authored material rates.
/// @param claims  This tick's claim list (`economy_report::budget_claims`);
///                derived claims are appended in walk order.
/// @return        The intents, in emission order (ascending nation, stone
///                then timber) — settle after the budget pass.
std::vector<network_purchase> derive_network_upkeep_claims(const world& w,
                                                           const std::map<entity_id, nation_budget>& budgets,
                                                           const network_upkeep_params& p,
                                                           std::vector<budget_claim>& claims);

/// Settle the derived intents against what `run_national_budget` actually
/// paid. A paid `logistics_maintenance` transfer funds the first unfunded
/// intent with the same (nation, supplier) — the pass keeps arrival order
/// within equal transfer keys and the gather keeps arrival order within a
/// corp's claims, so the sequence match is exact even when one supplier sells
/// both goods. Settlement draws quantity x the transfer's own
/// `fill_fraction` from the supplier's pool — the goods CEASE TO EXIST — and
/// records `paid` / `drawn` on the intent. A funded intent whose pool can no
/// longer cover the draw (unreachable in one pass by the derivation's
/// reservation; defended anyway), and a paid transfer on this line with NO
/// intent behind it (the claim vector is wire-reachable over --serve; no
/// in-process producer other than this TU claims on the line), are CLAWED
/// BACK whole: balance debited, treasury re-credited, the same two floats the
/// pass moved.
///
/// @param w         World; supplier pools, and on claw-back the two balances,
///                  are mutated. Nothing else.
/// @param purchases The intents `derive_network_upkeep_claims` returned;
///                  `funded` / `paid` / `drawn` / `completed` written in place.
/// @param tick      The budget pass's report for the same tick.
void settle_network_purchases(world& w,
                              std::vector<network_purchase>& purchases,
                              const national_budget_tick& tick);
