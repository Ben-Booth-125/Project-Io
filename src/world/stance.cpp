#include "stance.hpp"

#include <algorithm>

std::pair<entity_id, entity_id> canonical_pair(entity_id a, entity_id b)
{
    return (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);
}

bool is_hostile(const world& w, entity_id from, entity_id to)
{
    return w.corp_hostile_pairs.find({from, to}) != w.corp_hostile_pairs.end();
}

bool are_friends(const world& w, entity_id a, entity_id b)
{
    return w.corp_friend_pairs.find(canonical_pair(a, b)) != w.corp_friend_pairs.end();
}

bool has_pending_friendship_offer(const world& w, entity_id offerer, entity_id target)
{
    return w.corp_friend_offers.find({offerer, target}) != w.corp_friend_offers.end();
}

namespace
{
/// Both corps must exist in `w.corporations` and be distinct. Shared by every
/// verb below — validated first, before anything reads or writes a table.
bool valid_pair(const world& w, entity_id a, entity_id b)
{
    if (a == b)
        return false;
    if (w.corporations.find(a) == w.corporations.end())
        return false;
    if (w.corporations.find(b) == w.corporations.end())
        return false;
    return true;
}
} // namespace

stance_result declare_hostile(world& w, entity_id from, entity_id to)
{
    if (!valid_pair(w, from, to))
        return stance_result::rejected_invalid;
    if (is_hostile(w, from, to))
        return stance_result::rejected_state;

    // Validation is complete; every mutation below is unconditional and the
    // whole thing is atomic from a caller's point of view (invariant 2 — a
    // corp is never left reading as both hostile and friendly toward the
    // same target).
    w.corp_hostile_pairs.insert({from, to});
    w.corp_friend_pairs.erase(canonical_pair(from, to));
    // A pending offer in either direction is stale the moment either party
    // turns hostile — withdraw it rather than leave it live under a war.
    w.corp_friend_offers.erase({from, to});
    w.corp_friend_offers.erase({to, from});
    return stance_result::applied;
}

stance_result offer_friendship(world& w, entity_id offerer, entity_id target)
{
    if (!valid_pair(w, offerer, target))
        return stance_result::rejected_invalid;
    if (are_friends(w, offerer, target))
        return stance_result::rejected_state;
    if (is_hostile(w, offerer, target))
        return stance_result::rejected_state; // return_to_neutral first
    if (has_pending_friendship_offer(w, offerer, target))
        return stance_result::rejected_state; // already pending, not a re-send

    w.corp_friend_offers.insert({offerer, target});
    return stance_result::applied;
}

stance_result accept_friendship(world& w, entity_id offerer, entity_id target)
{
    if (!valid_pair(w, offerer, target))
        return stance_result::rejected_invalid;
    if (!has_pending_friendship_offer(w, offerer, target))
        return stance_result::rejected_state;

    w.corp_friend_offers.erase({offerer, target});
    w.corp_friend_pairs.insert(canonical_pair(offerer, target));
    return stance_result::applied;
}

stance_result return_to_neutral(world& w, entity_id corp, entity_id counterparty)
{
    if (!valid_pair(w, corp, counterparty))
        return stance_result::rejected_invalid;

    // Unilateral: `corp` clears its OWN directed hostility toward
    // `counterparty` (not the reverse direction — that is the other party's
    // to hold or release), the shared friendship row if one exists (either
    // party may dissolve a friendship), and any pending offer between the
    // two in either direction. rejected_state (mutating nothing) iff none of
    // the three existed.
    const bool had_hostility = is_hostile(w, corp, counterparty);
    const bool had_friendship = are_friends(w, corp, counterparty);
    const bool had_offer = has_pending_friendship_offer(w, corp, counterparty) ||
                            has_pending_friendship_offer(w, counterparty, corp);

    if (!had_hostility && !had_friendship && !had_offer)
        return stance_result::rejected_state;

    w.corp_hostile_pairs.erase({corp, counterparty});
    w.corp_friend_pairs.erase(canonical_pair(corp, counterparty));
    w.corp_friend_offers.erase({corp, counterparty});
    w.corp_friend_offers.erase({counterparty, corp});
    return stance_result::applied;
}
