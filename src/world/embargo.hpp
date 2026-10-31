#pragma once

#include "entity.hpp"

#include <cstdint>

struct world;

// ---------------------------------------------------------------------------
// Embargo (BL-1418; RELATIONS.md § 3 Embargo — an authored refusal)
// ---------------------------------------------------------------------------
// "When A embargoes B, no cargo B owns lands in a market on A's ground." A
// one-way bar between NATIONS, held in `world::nation_embargoes` as the pair
// (author, embargoed). It reads at the border the import tariff reads: a tariff
// prices B's goods in, an embargo refuses them.
//
// THE AUTHOR IS A NATION, ACTING FOR ITSELF (NATIONS.md § 4 Law authorship:
// "Enactment is the author nation's act"). It is enacted and repealed through
// one verb on the command seam (`corp_verb::set_embargo`); no scorer issues it,
// so no rival nation embargoes anyone (no grant in AI_OPPONENT.md § 11 covers it).
//
// Enactment is conduct: `embargo_imposed` is queued from the embargoed nation
// toward the author (RELATIONS.md § Who emits each factor), through
// `queue_conduct`, so the settle gate holds. A repeal emits nothing — no factor
// is authored for it.
//
// WHERE IT BITES. Two places, one rule (`embargo_refuses`):
//   * the border, `credit_arrived_convoys`: a convoy whose owner is embargoed by
//     the destination market's nation does not land. Its cargo is DESTROYED —
//     the interception path's fallback when a cut cargo has nowhere to land
//     (supply_system.cpp): no landing, no payment, no Traded-in, no duty, no
//     route record. The cargo was bought and its haul paid at dispatch, so the
//     loss is the owner's. Only cargo already in flight when the embargo is
//     enacted can reach this.
//   * dispatch, `commit_trade_shipment`: a shipment the border would refuse is
//     never bought, so a standing or auto trade does not pour goods into a closed
//     border every tick. A manual trade naming such a market is refused at the
//     seam with `rejected_embargo`; one already standing ships nothing while the
//     embargo holds and resumes when it is lifted.
// ---------------------------------------------------------------------------

/// Outcome of `set_embargo`. A rejection mutates nothing.
enum class embargo_result : uint8_t
{
    applied,
    rejected_invalid, ///< author or target not a nation, or the same nation.
    rejected_state,   ///< enacting an embargo already in force, or repealing one that is not.
};

/// True iff nation @p author embargoes nation @p target.
bool embargoes(const world& w, entity_id author, entity_id target);

/// Enact (@p on) or repeal (!@p on) @p author's embargo of @p target. Validates
/// everything before it writes. Enactment queues `embargo_imposed` (target ->
/// author); repeal queues nothing.
embargo_result set_embargo(world& w, entity_id author, entity_id target, bool on);

/// The nation whose ground market @p market's centre stands on, or null.
entity_id embargo_market_nation(const world& w, entity_id market);

/// True iff cargo owned by @p owner may NOT land in market @p market: the
/// market's nation embargoes the owner's nation (`owner_home_nation`). False on
/// an empty embargo set without a lookup, so a world with no embargo pays nothing.
bool embargo_refuses(const world& w, entity_id owner, entity_id market);
