#pragma once

#include "entity.hpp"
#include "sentiment.hpp"

#include <map>

struct world;
class recipe_registry;

// ---------------------------------------------------------------------------
// Sentiment emitters (BL-1458, BL-1432; RELATIONS.md § Who emits each factor,
// § 6 Alarm)
// ---------------------------------------------------------------------------
// The WORLD side of the relational substrate. `sentiment.cpp` is never handed a
// `world&` (THE INVARIANT: sentiment informs a declaration, never makes one), so
// everything that reads the world to decide that conduct HAPPENED lives here,
// and hands the substrate nothing but `sentiment_event`s.
//
// Every emitter GATHERS: it appends to `world::sentiment_pending` (or, for the
// trade flow, to `world::sentiment_trade_landed`). Nothing here writes a row
// directly. `fold_pending_sentiment` turns the gathered conduct into ONE batch a
// tick and hands it to `apply_sentiment_events`, which canonicalises it before
// folding, so the order emitters ran in is never observable.
//
// Nothing here reads or writes a stance table. A declaration is an INPUT to
// sentiment (Lane W's `declare_war` emits `hostility_declared`); sentiment is
// never an input to a declaration.
//
// One emitter, one call site:
//
//   trade_conducted      credit_arrived_convoys -> note_trade_landed
//   friendship_accepted  accept_friendship      -> queue_conduct (both rows)
//   force_used           run_battles            -> note_battle_losses
//   technology_visible   } emit_alarm_capability, once a tick from
//   facility_visible     } run_economy_step, after the end-of-tick
//   border_force         } re-anchor of the fronts
//   trade_severed        fold_pending_sentiment, through the trade streak's
//                        persistence window (sentiment_trade_streak)
//   embargo_imposed      NO EMITTER: the campaign has no embargo law and no
//                        enactment event for one (RELATIONS.md § 3). Authored
//                        at its directions so an emitter is one call when it lands.
//   unrest_visible       NO EMITTER: unrest has no store. Authored zero.
// ---------------------------------------------------------------------------

/// True outside the settle. Under it (`world::settle_build_gate`) every emitter
/// below gathers nothing and the fold folds nothing: the settle is not conduct,
/// so the opening world carries generation's grudges and no other sentiment.
bool sentiment_conduct_live(const world& w);

/// Queue ONE occurrence of @p kind from @p observer toward @p subject for the
/// next fold. Ill-formed events (null ids, a self pair, a non-finite magnitude)
/// are dropped by the fold itself, so a caller need not pre-filter.
void queue_conduct(world& w, entity_id observer, entity_id subject,
                   sentiment_factor_kind kind, float magnitude = 1.0f);

/// The trade flow's emitter: @p value credits of cargo owned by @p owner (a
/// nation, or anything whose home nation `owner_home_nation` resolves) landed in
/// market @p dest_market. A foreign landing — owner nation and the market's
/// nation both nations and different — accumulates on the pair; a domestic one,
/// or one with no nation on either end, is not trade between nations and is
/// ignored. Non-finite or non-positive values are ignored.
void note_trade_landed(world& w, entity_id owner, entity_id dest_market, float value);

/// The battle emitter (`force_used`): one battle day cost the attacker
/// @p attacker_lost men and the defender @p defender_lost. The side that lost
/// strength observes the other, magnitude the men it lost. Only nations observe.
void note_battle_losses(world& w, entity_id attacker, entity_id defender,
                        int attacker_lost, int defender_lost);

/// Alarm's capability factors (RELATIONS.md § 6), once a tick: every nation's
/// public technology tell, its large single-site facilities, and the force it
/// stands where a neighbour sees it - on the front they share and anywhere
/// inside the neighbour's ground (magnitude in thousands of men). Reads the
/// front cache, so call it after `reanchor_fronts`.
void emit_alarm_capability(world& w, const recipe_registry& reg);

/// Each nation's held `alarm_decay` multiplier, keyed by nation, for
/// `decay_sentiment`'s per-subject Alarm rate. Only nations whose scale is not
/// exactly 1 appear, so a world where nobody holds such a node gets an empty map.
std::map<entity_id, float> alarm_decay_scales(const world& w);

/// THE FOLD, once a tick: the trade accumulator becomes `trade_conducted`
/// events (each party toward the other, magnitude the value landed), an
/// established pair that has now landed nothing for the window becomes one
/// `trade_severed` (the receiver toward the shipper), every event is stamped with its subject's
/// `alarm_toward` scale, and the whole batch goes through
/// `apply_sentiment_events` once. The pending queue and accumulator are then
/// cleared. Returns the number of events folded (before the substrate's own
/// well-formedness filter).
std::size_t fold_pending_sentiment(world& w, const recipe_registry& reg);
