#pragma once

#include "charter_budget.hpp"
#include "recipe_registry.hpp"
#include "world.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

struct generation_progress; // hard_coded_world.hpp; the loading screen's write-only tap

// ---------------------------------------------------------------------------
// Procedural corporation generation
//
// Deterministic, five-pass corporation generation for the campaign start state.
// Runs after nation generation; reads nation_component and tile_component data
// from `w` and writes corporation_component entries plus building/stockpile
// components back into `w`. Also sets w.player_entity.
// See docs/generation/CORPORATION_GENERATION.md for the design authority and
// per-pass rules.
// ---------------------------------------------------------------------------

/// Tunable parameters for a corporation generation run.
struct corporation_params
{
    /// Total number of corporations to generate (including the player's).
    /// The prototype targets 6–10 on Kepler.
    int corporation_count = 8;

    /// Whether generation hands every specialist corporation an opening military
    /// base and a 50-head unit on it (`seed_starting_military`, BL-324).
    ///
    /// **Default FALSE from 2026-08-26 (Ben): a new charter does not open with a
    /// standing army.** The seeding itself is unchanged and is NOT deleted — this
    /// is the "shape stays, the number changes" idiom the upkeep rates already
    /// use, so the behaviour is one flag away and `rival_military_seeding_harness`
    /// can still exercise it by setting this true.
    ///
    /// Why it went off: under BL-454's standing-force upkeep a seeded unit costs
    /// **7.5 cr/qtr** from turn one, measured 2026-08-26 as **16.5 % of the seated
    /// corp's operating outgoings** and 19–31 % of its entire operating gap — for
    /// a regiment a brand-new charter never asked for and cannot use. It is
    /// symmetric: rivals lose it too, so the opening stays level, and a corp that
    /// wants force still raises it through `hire_unit` like anything else.
    bool seed_starting_force = false;

    /// Baseline starting capital before wealth variance is applied.
    ///
    /// **400 (Ben, 2026-08-26), superseding the 0 of 2026-07-06.** The zero was a
    /// deliberate "model every corp as a new charter — capital is *earned*, not
    /// granted" steer, and the earning half of it still holds: the opening balance
    /// is still overwhelmingly the pre-game ticks' doing (the eighty-quarter warm
    /// start when this was measured; now the winner's twelve-tick settle,
    /// `k_campaign_settle_ticks` in world/campaign_settle.hpp, BL-978 — against the generation-time asset
    /// placement). What the zero did NOT anticipate is what a zero buffer does when
    /// combined with compounding debt interest.
    ///
    /// Measured 2026-08-26 across 12 seeds: with no cash at all, a corp's first
    /// negative quarter puts it underwater on tick 1 and it compounds for the other
    /// 79. Nine of twelve seeds were marked (dip), and interest was then **43-60% of
    /// their entire loss** against an operating gap of only -24 to -40 cr/qtr. The
    /// capital is a **buffer against the spiral**, not a subsidy: it does not make a
    /// loss-making corp profitable, it stops a survivable bad quarter from becoming
    /// an unrecoverable one. Ben's framing: *"losses should be salvageable... it's
    /// not fun to see an inevitable loss."*
    ///
    /// (History: 100000 originally, dwarfing building costs 200-1000x; 4000 interim;
    /// 0 from 2026-07-06; 400 from 2026-08-26.)
    /// `compute_capital`/`compute_starting_stockpile` guard base_capital <= 0, so the
    /// zero remains a supported value rather than a special case.
    float base_capital = 400.0f;

    /// Fractional spread around base_capital. A value of 0.4 means each
    /// corporation's capital is drawn from [base × (1 − 0.4), base × (1 + 0.4)].
    float wealth_variance = 0.4f;
};

/// Generate corporations over the nation map already in @p w and register all
/// results in @p w. Also sets w.player_entity to the entity id of the
/// corporation flagged as the player's.
///
/// Runs the five-pass corporation generation pipeline (nation assignment,
/// industrial focus, starting asset placement, financial profile, naming)
/// described in docs/generation/CORPORATION_GENERATION.md. Requires that
/// w.nations is non-empty (i.e. generate_nations has already been called);
/// returns {} immediately if it is empty.
///
/// Deterministic in @p seed: the same seed, nation map, and params always
/// produce the same set of corporations. Does not use std::random_device or
/// global state.
///
/// @param w      World holding nation and tile components; receives new
///               corporation entities, building entities, and stockpile entries.
///               w.player_entity is set before this function returns.
/// @param params Tunable generation parameters.
/// @param seed   Per-run RNG seed for independent, reproducible results.
/// @param settle Optional settlement/industrialisation record for the home body
///               (BL-219). When supplied, Pass 2 stops drawing a focus from the
///               authored weighting table and DERIVES it from the region the
///               corporation is anchored to: a region that industrialised
///               around ore emits extraction/processing corps, a coastal trade
///               region emits trade-focused ones, and an early industrialiser
///               skews one tier further up the value chain. Diversity then comes
///               from a world-level reject-and-reroll against a floor on the
///               SET — never a quota on any member — so Pass 2's
///               diversity-without-quota property survives the rewrite. Null
///               (the default) keeps the pre-BL-219 authored table, so a body
///               with no settlement pass generates exactly as before.
/// @param progress Optional progress sink (BL-305). When non-null, Pass 3
///               publishes each holding's grid position as it is staked (the
///               POSITIONAL half — it lands on the loading screen's carve map)
///               and Pass 5's assembly publishes each corp's focus / holdings /
///               capital (the NON-POSITIONAL half — a financial profile has
///               nowhere on a map to be, so it goes to a ledger column). A pure
///               TAP: write-only, consumes no randomness, changes no branch.
///               Defined in world/hard_coded_world.hpp.
/// @param reg    Optional recipe registry (BL-1188, seat kit runs). When given,
///               Pass 3 is CHAIN-FEASIBLE (CORPORATION_GENERATION.md § Pass 3):
///               each processor placed is given a recipe whose inputs are
///               produced within reach of its market, or is unplaced, and a rung
///               left with nothing widens. Null (world generation's own call,
///               before any registry exists) leaves processors `no_recipe`.
/// @return       Corporation entity IDs in generation order (one per corporation
///               created). The entry whose corporation_component::is_player is
///               true equals w.player_entity.
std::vector<entity_id> generate_corporations(
    world& w,
    const corporation_params& params,
    uint32_t seed,
    const struct settlement_state* settle = nullptr,
    struct generation_progress* progress = nullptr,
    const recipe_registry* reg = nullptr);

/// BL-977 — strip the SPECIALIST roster so a candidate can lay a fresh one.
///
/// `generate_corporations` APPENDS, and has already run inside
/// `make_hard_coded_world` by the time the landscape search sees the world; a
/// second call would double every specialist. This is the inverse the roster
/// axis needs: every corporation with `is_background == false` goes, with its
/// asset buildings and their stockpiles, its body pools, any units it owns and
/// its per-corp tech/modifier rows; `player_entity` is cleared when it named
/// one of them (the seat is drawn afterwards, from the roster that survives).
/// BACKGROUND FIRMS ARE UNTOUCHED — they are laid by the candidate's own
/// placement pass and belong to it, not to the world-gen roster.
///
/// Walks corporations in ascending id, so the erase order — and the entity ids
/// the regenerated roster then draws — is the same on every standard library.
/// Invalidates the logistics caches: a removed port or hub was a supply anchor.
///
/// What it does NOT undo: on a world with no charter budget the market carving
/// already read where the world-gen roster clustered (`corps_in_nation`), and
/// markets are settled by the end of phase 4 — the search moves rosters over
/// fixed markets by design. ON A BUDGET WORLD THERE IS NOTHING TO REMOVE
/// (BL-1086): generation lays no world-gen roster there, and the carve counts
/// the budget's planned charters (`plan_charters_by_nation`) instead, so this
/// removes nothing and the budget's web is the only roster the world carries.
///
/// @return The number of corporations removed.
int remove_specialist_roster(world& w);

/// BL-1154 (Ben, 2026-10-01, NR-963 A; MILITARY.md § "BL-476 rivals start
/// armed") — THE OPENING FORCE: rivals start armed, the seat opens unarmed.
///   * `arm_rivals`: every non-background corporation but `w.player_entity`
///     without a military base gets one beside its HQ and a 50-head unit
///     (`seed_starting_military`), in ascending corporation id. Background
///     firms stay unarmed. Draws no randomness.
///   * `arm_corporation`: the same for one corporation (no-op for a background
///     firm or one already armed).
///   * `disarm_corporation`: removes a corporation's units and military bases
///     (buildings, stockpiles and asset entries), ascending id.
/// Generation arms the rivals where it charters (`generate_corporations`,
/// `charter_web_from_budget`); `repoint_player` (spawn_seat.cpp) disarms the
/// seat when the Begin pick moves it, and arms the corporation it leaves.
///   * `move_seat_force`: THE ONE SEAT-MOVE RULE, used by both ways a seat is
///     taken — the draw (`repoint_player`, spawn_seat.cpp) and the pick
///     (`corp_verb::take_seat`, corp_command.cpp). When the seat moves from
///     @p previous to @p corp, @p corp is disarmed and @p previous armed; a
///     no-op when they are the same corporation.
///
/// WHEN IT RUNS, AND WHAT THAT MEANS: the seat is taken at Begin, AFTER the
/// twelve-tick settle, so this is not "as if generation". The corporation the
/// seat leaves is armed then: a base on the nearest valid tile to its HQ on the
/// world as it stands, and a unit, both with NEW entity ids; it never hired in
/// the settle (it was the provisional player). The seat's own units and bases go
/// with their ids. Accepted (BL-1154 review, 2026-10-02).
bool corporation_has_opening_force(const world& w, entity_id corp);
void move_seat_force(world& w, entity_id previous, entity_id corp);

/// BL-1206 — THE SEAT OPENS WITH A CLEAN SLATE OF CONSTRUCTION (Ben, 2026-10-07;
/// CORPORATION_GENERATION.md § The spawn shortlist, and the seat, step 5). Every
/// building of @p corp still under construction (`ticks_remaining > 0`) is
/// cancelled — removed through `demolish_building`, so no building, stockpile or
/// asset entry survives — in ASCENDING building id, and what each had been
/// charged so far (`building_component::construction_paid`, the site's own
/// record of every credit run_construction took for it) is refunded to the
/// corporation's balance. Finished buildings stay. The refund is booked on the
/// corporation's next quarterly return as `refunds` (FINANCE.md § The quarterly
/// return) through `corporation_component::refund_unbooked`.
///
/// Called by both ways a seat is taken — the draw (`repoint_player`) and the
/// pick (`corp_verb::take_seat`) — BEFORE `move_seat_force`, so a muster base
/// still under construction is refunded rather than disarmed unpaid.
/// Deterministic and draw-free. Returns the credits refunded.
float seat_clean_slate(world& w, entity_id corp);
void arm_rivals(world& w);
void arm_corporation(world& w, entity_id corp);
void disarm_corporation(world& w, entity_id corp);

/// BL-1032 — CHARTER THE WEB FROM A PER-CENTRE BUDGET (INDUSTRIALISATION.md § 1;
/// CORPORATION_GENERATION.md Pass 1 and Pass 6, both AMENDED FORWARD). Lays
/// specialists AND background firms around the population centres @p budget
/// names, in place of `generate_corporations` + `generate_background_firms`.
/// The caller (`apply_landscape_candidate`'s budget overload) has already run
/// `remove_specialist_roster`; this function appends.
///
/// NEW CODE BESIDE THE LEGACY PASSES, NEVER A REFACTOR OF THEM: it reuses this
/// file's helpers (placement, capital, stockpile, naming, HQ, the gap
/// selection's measurements) and edits neither legacy body, so a world with no
/// budget keeps its bytes.
///
/// THE SPEND, in order:
///  * Refused params (`charter_spend_refusal`) charter NOTHING and touch
///    nothing; the report is `charter_refused_report`. (The landscape overload
///    decides a refusal before any mutation and never calls this with one.)
///  * Centres spend by budget DESCENDING, ties to the lower centre id, in ONE
///    walk (INDUSTRIALISATION.md § 1: a centre "charters exactly one specialist; what
///    remains buys background firms around it"): each centre's specialist, then
///    that centre's firms, then the next centre.
///  * Home nation = `tile_to_nation` of the centre tile; none -> the whole
///    budget unspent (`no_nation`).
///  * TWO REGIONS. CHARACTER (a specialist's focus and ownership) is
///    `nearest_region` reconciled to that nation — a mismatch takes the nation's
///    own nearest region to the centre; with none, or no settlement, the
///    national-character fallback. ANCHORING (rung 2) is `nearest_region` only
///    when that region is the centre nation's own, and otherwise nothing.
///  * A centre whose budget >= its specialist price (`firm_price_of(centre) x
///    specialist_firm_charters`) charters EXACTLY ONE specialist: focus and
///    ownership from the character region (Passes 2 and 2b), today's capital
///    (400 +/- 40%, the focus premium included), stockpile, name and HQ. NO
///    nation balancing, no diversity reroll. The remainder buys background
///    firms at the firm price by Pass 6's gap selection — construction first,
///    then the biggest gap under the body's per-good cap (`per_resource_firm_cap`
///    under `fixed`, none under `lifted`), or under `sqrt_capital` the goods IN
///    TURN, a firm per good each pass up to the square-root cap (BL-1039) — under
///    the per-province cap when `spend.province_cap` (2 per rung of the
///    province's centre, BL-1146 — `province_firm_cap`), the
///    `density_ceiling` under `sqrt_capital`, and the `max_firms_per_body`
///    runaway guard. Each body's firm points B, its goods with demand G, B_ref
///    and the per-good cap are FIXED BEFORE THE WALK (`charter_sqrt_per_good_cap`).
///  * Anchor rungs: the centre nation's tiles within `spend.window_radius` of
///    the centre tile (column-wrapped), then the anchor region's window (empty
///    when the nearest region is a neighbour's), then UNSPENT — never nation-wide.
///    The rungs bound the ANCHOR only; secondary holdings walk outward from it
///    (`place_starting_assets`, unchanged), and each record lists every holding
///    tile so a reader can measure the spill.
///  * What is not spent is counted by reason: `window_exhausted`,
///    `province_cap` (the windows had anchorable ground and the cap took all of
///    it), `no_gap`, `body_cap`, `density_ceiling`, `remainder` — never
///    scattered. The report carries each body's rule and its firms per good.
///  * The player is a seeded pick among the budget's specialists; with none,
///    nobody is picked and the report says `no_specialists`. On the shipped
///    seam a budget no centre can buy a specialist with never gets here (NR-910,
///    `charter_budget_affords_specialist`); a walk that charters none anyway is
///    the residual, reported and not patched.
///
/// RNG: a fresh std::mt19937 per centre per role, seeded through a keyed
/// `checkpoint_rng` draw on salts no other generation stream uses, so one
/// centre's draws never depend on another's success.
///
/// @param seed    The candidate's placement seed (as the legacy passes take it).
/// @param settle  The settlement record (`world::gen_settlement`), or null.
/// @param report  Optional; overwritten with the spend's report.
/// @return        Every corporation chartered, ascending id.
std::vector<entity_id> charter_web_from_budget(world& w,
                                               const recipe_registry& reg,
                                               const charter_budget& budget,
                                               const charter_spend_params& spend,
                                               uint32_t seed,
                                               const struct settlement_state* settle,
                                               charter_spend_report* report = nullptr);

struct lapse_event;

/// BL-1099 — DATE THE CHARTERS AGAINST THE RECORD (CORPORATION_GENERATION.md
/// § The spawn shortlist; INDUSTRIALISATION.md § Beat 1 "Works chartered").
/// The walk above stamps each firm's `origin_region` but cannot see the
/// Industrialisation record; this pass, run once on the winner's report by
/// `finish_campaign_world`, gives every charter in @p report its year: a
/// region's k-th charter (in the report's own spend order, richest centre
/// first) takes the year of the region's k-th `works_chartered` note in
/// @p events; past the notes, the region's first `furnace_lit` year; never
/// lit, @p epoch_year. Written onto the firm (`corporation_component::
/// founded_year`) and onto the report row (`charter_record::founded_year`)
/// alike. Pure over its inputs and order-independent: the maps it builds are
/// read by key only. A firm the report names that the world no longer holds
/// is skipped; a firm with no origin opens at the epoch.
void date_chartered_firms(world& w, charter_spend_report& report,
                          const std::vector<lapse_event>& events, int32_t epoch_year);

/// BL-1060 — the refusal a spend's params cannot decide alone: under
/// `sqrt_capital`, a density ceiling that BINDS on some budgeted body and yet
/// leaves under one firm per turn good once the yards' places come off it, so
/// NR-905's reservation could not be cut at all. Null when the spend may go
/// ahead (and for an empty budget, a legacy rule, or params
/// `charter_spend_refusal` already refuses).
///
/// READ-ONLY, and read BEFORE ANY MUTATION: `apply_landscape_candidate` and
/// `search_landscape` ask it on the world as they receive it, beside
/// `charter_spend_refusal`, and `charter_web_from_budget` asks it again at its
/// top — on the world the walk itself will spend, after the budget apply has
/// removed world-gen's roster. That second ask is the authority (BL-1060 round
/// 4): the walk never runs a body whose shares could not be cut, whatever the
/// earlier ask saw.
const char* charter_spend_world_refusal(const world& w, const recipe_registry& reg,
                                        const charter_budget& budget,
                                        const charter_spend_params& spend);

/// NR-910 — THE NO-SPECIALIST WORLD, decided BEFORE ANYTHING IS CHARTERED (Ben,
/// 2026-09-21: "a world whose budget opens no specialist falls back to the world
/// it would have built with no budget, exactly as a refused spend does, decided
/// from the budget before anything is chartered"). True when some budgeted
/// centre stands on a tile a nation owns and its points cover the specialist's
/// price at its own reach (`spend.specialist_price_of`, BL-1168) — the walk's own test for chartering
/// one, on the walk's own resolution of the centre. False for an empty budget.
///
/// READ-ONLY. It reads the centre tiles, their ownership and the nations, none
/// of which the budget apply moves before the walk (the road tier and the roster
/// removal touch neither), so the answer on the world as `apply_landscape_candidate`
/// and `search_landscape` receive it is the answer the walk would reach.
///
/// It decides AFFORDING, not PLACING: a centre that affords a specialist and
/// finds no ground in either window charters none, and a world where every
/// affording centre does so is the walk's residual (`charter_spend_report::
/// no_specialists` with neither `refused` nor `fell_back`).
bool charter_budget_affords_specialist(const world& w, const charter_budget& budget,
                                       const charter_spend_params& spend);

/// BL-1146 (Ben, 2026-09-27, NR-960 B, superseding NR-910's flat 2;
/// INDUSTRIALISATION.md § 1, "The per-province cap scales with the province's
/// centre") — THE PER-PROVINCE FIRM CAP. Two firms per rung the province's centre
/// reaches: a village 2, a town 4, a city 6, a metropolis 8, a megacity 10. A
/// province is its centre's whole ground (PROVINCES.md), so a flat 2 pushed a
/// city's industry out into its villages. A BUDGET-WORLD RULE (INDUSTRIALISATION.md
/// § 1): read by the charter budget's walk (`charter_web_from_budget`) and nothing
/// else. Pass 6 (`generate_background_firms`), which runs on every world without
/// a budget, keeps its flat 2 — those worlds' bytes are BL-1031's pinned contract.
/// A rung outside 1-5 reads as the nearest end: a province with no centre is a
/// village's 2.
inline constexpr int k_province_firm_cap_per_rung = 2;

constexpr int province_firm_cap(int centre_rung)
{
    return k_province_firm_cap_per_rung * (centre_rung < 1 ? 1 : centre_rung > 5 ? 5 : centre_rung);
}

/// BL-1146 — every province's CENTRE RUNG (1-5), keyed by province id. "The
/// province's centre" is its ANCHOR, read from `province_anchors` (province.hpp,
/// BL-611; PROVINCES.md) — the one derivation `seed_province_holders` reads: the
/// highest SUMMED centre scale standing on one tile of the province, ties to the
/// lowest tile id (a razed centre keeps scale 1, so it still anchors). The rung
/// is that sum on the ladder's 1-5, clamped: two centres sharing a tile could sum
/// past a megacity. A province absent from the map carries no centre (the
/// village's cap). READ-ONLY, pure, integer. The budget walk builds it ONCE: the
/// walk charters buildings, never centres, so no rung moves under it.
std::map<std::uint32_t, int> province_centre_rungs(const world& w);

/// The cap for province @p province under @p rungs (`province_centre_rungs`):
/// `province_firm_cap` of its rung, the village's for a province with no centre.
int province_firm_cap_of(const std::map<std::uint32_t, int>& rungs, std::uint32_t province);

/// BL-1086 (the review's fix round) — THE BUDGET-WORLD TEST, ONE PREDICATE. Is
/// @p w, with @p budget charged at @p spend, a world whose whole web is chartered
/// from the budget? Asked in three places that must agree — generation's bump 11
/// (whether to lay a roster, and what the carve counts), `search_landscape`
/// (whether the roster axis is skipped) and `apply_landscape_candidate`'s budget
/// overload (which branch lays the candidate) — so it is written once, here, in
/// the order all three always read it:
///
///   1. no budget, or an EMPTY one (the all-zero budget is the same state) ->
///      `no_budget`: today's world, nothing else read;
///   2. `charter_spend_refusal` (the params) -> `refused`;
///   3. `charter_spend_world_refusal` (the world: a density ceiling too small for
///      a body's turn and its yards, BL-1060) -> `refused` — ASKED ONLY WITH A
///      REGISTRY (@p reg non-null);
///   4. no centre a nation owns affords a specialist (NR-910) -> `no_specialist`;
///   5. otherwise `budget`.
///
/// WHY BUMP 11 PASSES NO REGISTRY, AND WHY THAT CANNOT MISLEAD IT. Generation is
/// Lua-free and takes no recipe registry — the caller loads one from Lua after
/// the world exists (app::load_economy, `finish_campaign_world`) — and the world
/// refusal needs one: it reads the yards' places off the registry's construction
/// recipe and the body's demand off its baskets. So bump 11 skips step 3.
/// Under the SHIPPED constants step 3 cannot fire on a body serving 7 or more
/// goods. It refuses a body only when the ceiling binds AND (ceiling -
/// yard_places) < n_turn, with yard_places <= the per-good cap
/// (`charter_yard_places` takes the minimum with it). The cap is
/// max(c, isqrt(c x F / |G|)) for F the body's whole firm charters; F < 2 x
/// `k_stockpile_price_divisor` (the firm price is the stock / the divisor,
/// rounded down, so the stock buys fewer than twice the divisor), so with
/// c = 8 the cap is at most isqrt(8 x 1299 / |G|), and n_turn <= |G|. The
/// ceiling is per good served (BL-1204): floor(7.5 x |G|) under the guard. At
/// |G| = 7, n_turn + cap <= 7 + 38 = 45 against a ceiling of 52, and past it the
/// cap only falls while the ceiling rises, so no body serving 7+ goods is ever
/// uncuttable. BELOW 7 THE BOUND DOES NOT HOLD (|G| = 6: 6 + 41 = 47 against
/// 45): a body serving 1-6 goods whose budget outruns its small ceiling and
/// whose yards' places eat it could refuse the whole spend. A budgeted body —
/// one carrying a population centre — serves far more (its baskets alone, plus
/// their inputs), so this is a bound on the argument, not a seen case; the
/// harness row that would name it is landscape_search_harness's L1. A world
/// that hit it would be one bump 11 calls a budget world and the search does
/// not — the carve would then have read planned firms and the search's no-budget
/// branch would lay the roster itself (so the world still gets one).
enum class budget_world_kind : std::uint8_t
{
    no_budget     = 0,
    refused       = 1,
    no_specialist = 2,
    budget        = 3,
};

struct budget_world_reading
{
    budget_world_kind kind    = budget_world_kind::no_budget;
    /// `refused`: why, from `charter_spend_refusal` or `charter_spend_world_refusal`.
    const char*       refusal = nullptr;
    bool budget_world() const { return kind == budget_world_kind::budget; }
};

/// The five steps above. @p reg null skips step 3 (bump 11 only).
budget_world_reading read_budget_world(const world& w, const charter_budget* budget,
                                       const charter_spend_params& spend,
                                       const recipe_registry* reg);

/// `read_budget_world(...).budget_world()`.
bool is_budget_world(const world& w, const charter_budget* budget,
                     const charter_spend_params& spend, const recipe_registry* reg);

/// BL-1086 — THE BUDGET'S PLANNED CHARTERS, per nation (Ben, 2026-09-26, option
/// A; MARKETS.md § Market centres and seeding): the corporations each nation's
/// centres will charter, which is what the market carve counts as a nation's
/// competitors on a world whose firms come from the charter budget.
///
/// THE WALK'S OWN ARITHMETIC, IN THE WALK'S OWN ORDER (the review's fix round:
/// the first cut counted every firm a centre could afford, past the ceiling the
/// walk stops at). Per budgeted centre, on the walk's resolution (the centre's
/// tile, its body, and the nation owning it; a centre without one plans
/// nothing), spent BUDGET DESCENDING, TIES TO THE LOWER CENTRE ID:
///   * one SPECIALIST when its points cover `spend.specialist_price_of(centre)`;
///   * then its FIRMS — the points left, less any pooled remainder it sends plus
///     any it receives (NR-913, `plan_charter_pool`, the walk's own plan; zero
///     under the shipped `charter_pool::none`), in whole firm charters — each
///     firm counted only while its BODY is under the walk's stop: the runaway
///     guard `max_firms_per_body`, and under `sqrt_capital` the lower density
///     ceiling. A richer centre's firms therefore take a body's room before a
///     poorer centre's, exactly as the walk spends them. BL-1204: the shipped
///     ceiling is per good served, and |G| needs the recipe registry this plan
///     (and the carve that asks it) does not have, so the plan reads the ceiling
///     at |G| = `resource_count` — the guard less one under the shipped rate. A
///     fixed `density_ceiling` (an instrument's row) is read as it stands.
///
/// WHAT IT STILL CANNOT SEE: whether ground is found. The walk places each
/// charter, and a placement can fail (the windows, the province cap, the
/// per-good cap and the turn's shares); a failed firm is not chartered and does
/// not count toward the ceiling. So per BODY the walk charters AT MOST what this
/// plans (an upper bound), while per NATION a poorer nation can charter a
/// few more than planned where a richer centre's placements failed and left the
/// ceiling room it would have taken. landscape_search_harness's L1 reads both.
/// Placement is a function of the candidate's seed, which the carve cannot
/// know: the search runs after the carve, on the carve's markets.
///
/// READ-ONLY and pure: centre tiles, their ownership, the nations and (for a
/// pooled spend only) the markets.
struct charter_nation_plan
{
    std::int64_t specialists = 0; ///< centres that afford a specialist
    std::int64_t firms       = 0; ///< background firms, under each body's stop
    std::int64_t total() const { return specialists + firms; }
};

/// Keyed by nation id, ascending; a nation none of whose centres plans a charter
/// is absent. Empty for an empty budget or a firm price <= 0.
std::map<entity_id, charter_nation_plan> plan_charters_by_nation(
    const world& w, const charter_budget& budget, const charter_spend_params& spend);

/// BL-1086 (the review's R4) — THE CHARTER LEDGER ON THE LOADING SCREEN for a
/// budget world. Such a world lays no roster in generation, so
/// `generate_corporations` publishes no rows; the web the screen should show is
/// the search WINNER's, chartered after generation by `finish_campaign_world`.
/// This publishes it from the winner's spend report @p rep, onto the rows and
/// markers the screen already reads (`generation_progress::add_corp_row`,
/// `mark_asset`): one row per SPECIALIST in charter order (richest centre
/// first), its focus, holdings and starting capital, and a marker on each of its
/// holdings on the home body — the same three numbers and the same markers a
/// generated roster publishes, so the screen draws it unchanged. Background
/// firms are not rows, as they never were. `max_corp_slots` and
/// `max_asset_marks` drop the overflow, which is cosmetic. No row is marked as
/// the player's: the seat is drawn at Begin (`seat_player_corporation`), not
/// here. WRITE-ONLY, like every tap; null @p progress publishes nothing.
void publish_charter_web(generation_progress* progress, const world& w,
                         const charter_spend_report& rep);

// ---------------------------------------------------------------------------
// Pass 2b — ownership class (BL-631)
//
// Exposed rather than file-local for one reason: the ownership_class harness
// has to check the MAPPING, not just its end-to-end consequences, and a check
// that can only observe finished corporations cannot tell a correct derivation
// from a lucky one. Both functions are pure: no rng parameter, no world, no
// hidden state — which is what makes "the derivation consumes no additional
// randomness" a property of the signature rather than a claim about the body.
// ---------------------------------------------------------------------------

/// Pass 2b's derivation: the ownership class a corporation anchored in @p p
/// carries. Reads HISTORY.md's **Stage 1** — the enforceable promise — through
/// the charter's diffusion frame, never an authored table (BL-219's principle).
///
///  * the charter never reached here   -> `closed`  (no filing, no market in the firm)
///  * it reached here only as a COPY   -> `private` (it trades; its books are its own)
///  * this ground LIVES the promise    -> `public`  (it files, and it can be bought)
///  * lives it, but under a STATIST polity -> `private`
///
/// WHY STAGE 1 AND NOT STAGE 4 (BL-638). The obvious reading — early
/// industrialisers get public firms — is the wrong one, and it was measured
/// wrong once: an antiquity world skips Stage 4 entirely, so
/// `median_industrial_year` is 0 there and the never-industrialised rung fires
/// for every region in every default campaign. All 64 corporations across 8
/// seeds classed `closed`; nothing filed and nothing was buyable. Stage 1 runs
/// in every era, so a class derived from it is era-agnostic where a class
/// derived from industry is silently post-industrial.
///
/// @param p        The corporation's home region.
/// @param ch       `settlement_state::charter` — the charter's diffusion frame.
/// @param politics The home NATION's ideology. It supplies only the
///                 holds-the-firm-close DEMOTION; the region stays the primary
///                 discriminator, so two corps in one nation still differ
///                 (CORPORATION_GENERATION.md § Pass 2 - "per-region, not
///                 per-nation, and that is the point").
ownership_class ownership_from_region(const struct region& p,
                                      const struct charter_reach& ch,
                                      ideology politics);

/// The no-home-region fallback: the same read one grain up, taken from the
/// national character alone. Used by the rung-3 case (a nation the settlement
/// pass never reached) and by the whole no-settlement path. Nothing here
/// branches on `is_background`; the flag is not an input to this function.
///
/// BL-678 (companies are open): `generate_background_firms` still EVALUATES this
/// for every company it authors — the derivation is pure and draws nothing, so
/// the RNG stream is identical either way — and then OVERRIDES the result with
/// `publicly_held`. A company is open by construction; the class is a
/// CORPORATION's property. The override lives at that call site, not in here,
/// so this mapping stays the one thing it has always been.
///
/// The nation's `politics` IS its industrialisation-timing tercile
/// (settlement.cpp: never -> isolationist, early -> mercantile, mid ->
/// technocratic, late -> authoritarian), so this is the region mapping with the
/// tercile standing in for the region's own furnace year.
ownership_class ownership_from_character(ideology politics);


/// BL-365 — generate REAL background corporations (`is_background = true`) that
/// produce and consume through the normal recipe/workforce/market pipeline,
/// replacing the old abstract nation-substrate demand/supply injection
/// (BL-078's `inject_substrate_demand`, deleted). Reuses this file's own
/// clustered asset-placement machinery (`place_starting_assets`), so a
/// background firm's holdings read exactly like a rival's: a focus-shaped
/// cluster anchored on a scored tile, not a scatter.
///
/// MUST run after `generate_corporations` (so player/rival holdings claim the
/// choicest tiles first — background firms fill what's left) AND after the
/// recipe_registry is loaded from Lua, because the stop condition below reads
/// real recipe outputs. `make_hard_coded_world` runs BEFORE the Lua economy
/// layer loads (see app::setup_world / app::load_economy ordering, and the
/// identical constraint the pre-game warm start documents in app.cpp) — so,
/// unlike `generate_corporations`, this function is NOT called from inside
/// `hard_coded_world.cpp`. Callers invoke it once `reg` is loaded, mirroring
/// the pre-game-warm-start seam: `app::run` calls it right after
/// `load_economy()`, before the warm-start ticks (so the warm start also seeds
/// the new firms' opening balances); the headless `--serve` / blackboard-export
/// paths in `main.cpp` call it right after their own `reg.load_from_lua`.
///
/// MEASURED stop condition (the load-bearing decision, not a fixed firm count):
/// for each body carrying at least one population centre, spawns one firm at a
/// time — extraction or processing, whichever the current biggest per-resource
/// production/demand gap calls for — until aggregate real production (summed
/// from every building's actual recipe/extraction output, generated firms
/// included) reaches ~90% of aggregate demand (population + BL-340 background
/// pull, the same clearing_fraction the deleted substrate model used) over the
/// body's tradeable resource set, or a bound bites (max_firms_per_body /
/// max_iterations) — logged, not crashed, if 90% is unreachable (e.g. a body
/// missing the resource's deposit entirely).
///
/// Deterministic in @p seed (xor-offset from `generate_corporations`'
/// seed_asset stream so the two passes cannot collide).
///
/// @param w   World already carrying generated nations, corporations, tiles,
///            and markets (`generate_corporations` + market seeding must have
///            already run).
/// @param reg Loaded recipe registry — supplies recipe outputs, building
///            economics, population_demand and background_demand tunables.
/// @param seed Per-run RNG seed for deterministic, reproducible placement.
/// @return    Entity ids of the background corporations created (possibly
///            empty if every body is already at/above the clearing fraction,
///            or no body qualifies).
std::vector<entity_id> generate_background_firms(
    world& w,
    const recipe_registry& reg,
    uint32_t seed);

/// Give every processing facility that still carries `no_recipe` the registry's
/// era-aware default. Idempotent, and a no-op on a world where every processor
/// is already configured.
///
/// **Why this is a function and not three lines in the caller.** `author_building`
/// cannot set a recipe: a recipe id is an index into a registry that does not
/// exist yet when `make_hard_coded_world` runs (the Lua economy layer loads
/// after world generation — see app::setup_world / app::load_economy ordering).
/// So the field is left `no_recipe` and backfilled once the registry exists.
///
/// That backfill used to live inline in `app::load_economy`, which made a
/// **world-generation invariant depend on the UI's startup sequence**. Every path
/// that builds a world without going through `app` — every headless harness,
/// `--serve`, `--verify` — got generated processors that could never produce, and
/// nothing said so: they report as idle with `no_recipe`, which reads as an
/// economic outcome rather than a missing initialisation. Measured at **20.3% of
/// all processing building-ticks** in `tier_margin` before this moved (2026-08-17),
/// silently dragging down every mean the BL-436 calibration was being read off.
///
/// Call it after the registry is loaded and after every generation pass that can
/// author a processor (`generate_corporations`, `generate_background_firms`).
void assign_default_recipes(world& w, const recipe_registry& reg);

/// BL-1185 (chain-feasible placement) — Pass 3's rule applied to a specialist
/// roster laid BEFORE a recipe registry existed (world generation's own Pass 3,
/// which is Lua-free and runs before the registry loads). Called by every path
/// that keeps that roster once the registry is in hand: the headless run and
/// run_verify when the charter budget is empty, and the landscape apply when it
/// does not regenerate the specialists.
///
/// ONLY THE INFEASIBLE ARE RE-DECIDED. First the greatest set of the roster's
/// processors whose CURRENT recipes are feasible against producers that are
/// themselves kept is found (a fixed point: a processor whose recipe fails is
/// suspended — it supplies nobody — and the test repeats until nothing more
/// falls); those keep their recipes. Then each suspended processor, in (corp
/// id, asset order), takes the feasible recipe nearest its feed (own, market,
/// reach) as fresh placement would, or is unplaced. IDEMPOTENT: a second call
/// changes nothing, and no roster processor is left infeasible.
///
/// A corporation that lost a processor keeps the rest of its holdings (it
/// exists; it is not a charter that can be refused); its HQ is designated again
/// over its non-military holdings and its opening pools re-keyed to that HQ. If
/// the SEATED specialist is left holdless, or a processing corporation with no
/// processor, the seat is drawn again over the specialists that still qualify
/// with world-gen's own pick stream (`seed ^ 0xF0E1D2C3`); none qualifying
/// leaves the world with no seat (`seat == null_entity`), which callers report.
struct chain_roster_enforcement
{
    int processors_redecided = 0; ///< suspended processors given a feasible recipe
    int processors_unplaced  = 0; ///< suspended processors no recipe could feed
    int holdless             = 0; ///< specialists left with no (non-military) holding
    bool      seat_redrawn   = false;
    entity_id seat           = null_entity; ///< the seat after the call
};
chain_roster_enforcement enforce_chain_feasible_roster(world& w, const recipe_registry& reg,
                                                       std::uint32_t seed);

/// BL-1185 (chain-feasible placement) — the AUDIT of Pass 3's rule on a built
/// world: every standing processor (not decommissioned) checked against every
/// producer standing NOW, with the same reach placement uses
/// (`price_market_export_leg`). Weaker than the placement-time test, which sees
/// only what stood before the processor; a processor counted infeasible here
/// broke the rule outright. A measurement seam for harnesses — read-only on the
/// simulation, but it WARMS the logistics caches, so a caller that goes on to
/// tick should `invalidate_logistics_caches` after it.
struct chain_feasibility_audit
{
    int processors        = 0; ///< standing processors
    int no_recipe         = 0; ///< of which carry no recipe
    int infeasible        = 0; ///< of which some input has no producer within reach
    int infeasible_held   = 0; ///< of `infeasible`, held by a corporation
    int processors_held   = 0; ///< standing processors held by a corporation
};
chain_feasibility_audit audit_chain_feasibility(world& w, const recipe_registry& reg);

/// Measurement seam (2026-08-20) — the SHIPPED coverage arithmetic, readable
/// from outside. `generate_background_firms` stops on its caps (per resource,
/// per province, `max_firms_per_body`), never on a coverage target; the basket-
/// weighted production/demand ratio is what says whether a body's markets open
/// stocked or thin, and nothing outside that file could previously ask it.
///
/// Exported rather than re-derived on purpose: a harness that re-implements a
/// generation rule drifts from it, which has now cost this project four wrong
/// confident measurements (world_audit.cpp B4 is the latest).
///
/// All three are pure reads over @p w and @p reg. A body with no measurable
/// demand reads as fully met (ratio 1.0) — there is nothing to fill.
std::array<float, resource_count> measure_body_demand(const world& w,
                                                     const recipe_registry& reg,
                                                     entity_id body_id);
std::array<float, resource_count> measure_body_production(const world& w,
                                                          const recipe_registry& reg,
                                                          entity_id body_id);
float measure_production_ratio(const world& w, const recipe_registry& reg,
                               entity_id body_id);

/// BL-1232 (power plants per grid; PRODUCTION.md, "Generation is sized per grid,
/// not per body"; LOGISTICS.md § 3a). Generation's per-grid power measure: the
/// charter walk sizes power on it. Exported so harnesses read the walk's own
/// figure. (A scorer half that would also read it is HELD, Ben 2026-10-08, and
/// kept on branch bl1232-scorer-gate.)
///
/// `body_power_grid_gap`: @p body_id's power gap, the sum over its wired grids
/// of max(0, need - output). Need is the operating buildings' power upkeep,
/// keyed by each building's own tile grid (the draw side). Output is every
/// non-decommissioned generator at the nominal rate, keyed by the grid its
/// MARKET CENTRE is on (`tile_feed_power_grid`: where its listings land, the
/// feed side) — plants UNDER CONSTRUCTION included, so a plant already started
/// is counted as the supply it will be. A dark building neither draws nor
/// feeds. A grid needing under half of @p plant_output is left to roads, not
/// counted. @p short_grids receives every counted (short) grid;
/// @p unpowered_short, when given, the short grids no generator feeds at all
/// (PRODUCTION.md, "Unpowered grids first", Ben 2026-10-08). Deterministic
/// (ascending building id, std::map over grids).
float body_power_grid_gap(world& w, const recipe_registry& reg, entity_id body_id,
                          float plant_output, std::set<std::uint32_t>& short_grids,
                          std::set<std::uint32_t>* unpowered_short = nullptr);

/// The grids a power firm may SERVE on @p body_id (into @p serve), and the
/// body's power gap (returned, every short grid's, as `body_power_grid_gap`).
/// UNPOWERED GRIDS FIRST (PRODUCTION.md, Ben 2026-10-08: "every grid gets a
/// plant before any gets a second"): while any short grid has no power firm
/// chartered on it (@p chartered, the walk's own record) and no generator
/// feeding it, only those; else every short grid. A grid is powered the moment
/// a power firm is chartered on it, live output or not. The charter walk cuts
/// a power firm's windows to ground FEEDING one of them.
float power_grids_to_serve(world& w, const recipe_registry& reg, entity_id body_id,
                           float plant_output, std::set<std::uint32_t>& serve,
                           const std::set<std::uint32_t>* chartered = nullptr);

/// The most power one plant makes: the largest power output of any in-band
/// processing recipe at the nominal run. 0 where no recipe makes power.
float one_power_plant_output(const recipe_registry& reg);

/// True where power is sized per grid at all: some recipe makes power AND the
/// band authors a power upkeep draw on some building type. Elsewhere the per-grid
/// measure is not read and the charter walk behaves as before it existed.
bool power_sized_per_grid(const recipe_registry& reg);
